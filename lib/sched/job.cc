/*
 * Copyright (C) 2009-2012, Gostai S.A.S.
 *
 * This software is provided "as is" without warranty of any kind,
 * either expressed or implied, including but not limited to the
 * implied warranties of fitness for a particular purpose.
 *
 * See the LICENSE file for more information.
 */

#include <libport/cstdlib>
#include <iostream>

#include <libport/bind.hh>
#include <boost/lambda/lambda.hpp>
#include <boost/mem_fn.hpp>

#include <libport/containers.hh>
#include <libport/debug.hh>
#include <libport/foreach.hh>
#include <libport/indent.hh>
#include <libport/separate.hh>

#include <sched/job.hh>

GD_CATEGORY(Sched);

namespace sched
{
  StopException::StopException(unsigned depth, boost::any payload)
    : depth_(depth)
    , payload_(payload)
  {
  }

  /*-----------------.
  | Job::Collector.  |
  `-----------------*/

  Job::Collector::~Collector()
  {
    foreach (rJob& child, *this)
      parent_->terminate_child(child);
  }

  void
  Job::Collector::collect()
  {
    for (iterator i = begin(); i != end(); /* nothing. */)
      if ((*i)->terminated())
        {
          parent_->terminate_child(*i);
          i = erase(i);
        }
      else
        ++i;
  }

  std::ostream&
  Job::Collector::dump(std::ostream& o) const
  {
    return o << "{" << libport::incendl
             // Explicit parameters required by MSVC 2005.
             << libport::separate<const Collector,
                                  std::ostream&(*)(std::ostream&)>
                                  (*this, libport::iendl)
             << libport::decendl << "}";
  }

  std::ostream&
  operator<< (std::ostream& o, const Job::Collector& c)
  {
    return c.dump(o);
  }


  /*------.
  | Job.  |
  `------*/

  unsigned int Job::alive_jobs_ = 0;

  std::ostream&
  Job::dump(std::ostream& o) const
  {
    o << "Job(" << this << ")"
      << " "
      << (child_job() ? "child job" : "root job")
      << " state: " << state_;
    if (!children_.empty())
      o << " Children:" << libport::incendl
        << "{" << libport::incendl
        << children_ << libport::decendl
        << "}" << libport::decindent;
    return o;
  }

  void
  Job::run()
  {
    // This function never terminates properly: it yields and is never
    // called back. So perform RAII in a subblock to ensure that GD_CATEGORY
    // releases its memory.
    {
      aver_eq(state_, to_start);
      GD_FINFO_DEBUG("job %s: run", this);

      // We may get interrupted during our first run, in which case
      // we better not be in the to_start state while we are executing
      // or we would get removed abruptly from the scheduler pending_
      // list.
      state_ = running;
      if (stats_.logging)
        stats_.last_resume = scheduler_.get_time();
      try
      {
        if (has_pending_exception()
            && dynamic_cast<SchedulerException*>(pending_exception_.get()))
          check_for_pending_exception();
        work();
      }
      catch (TerminateException&)
      {
        // Normal termination requested.
        GD_FINFO_DEBUG("job %s: TerminateException", this);
      }
      catch (StopException&)
      {
        // Termination through "stop" or "block" on a top-level tag,
        // that is a tag inherited at the job creation time.
        GD_FINFO_DEBUG("job %s: StopException", this);
      }
      catch (const exception& e)
      {
        // Rethrow the exception into the parent job if it exists.
        if (parent_)
        {
          parent_->async_throw(ChildException(e.clone()));
          // Warn the scheduler that the world may have changed.
          scheduler_.signal_work_next_round();
        }
      }
      catch (const std::exception& e)
      {
        GD_FERROR("job %s: Exception `%s' caught, losing it", this,
                  e.what());
      }
      catch (...)
      {
        // Exception is lost and cannot be propagated properly.
        GD_FERROR("job %s: Unknown exception caught, losing it", this);
      }
    }
    // Write stats for this last run
    if (stats_.logging)
    {
      libport::utime_t start_resume = scheduler_.get_time();
      stats_.job.running.add_sample(
        start_resume - stats_.last_resume);
      // Just a precaution in case we end up in resume_scheduler_.
      stats_.last_resume = start_resume;
    }
    copy_stats_to_parent();
    terminate_cleanup();

    // We should never go there as the scheduler will have terminated us.
    pabort("scheduler did not terminate us");
  }

  void
  Job::terminate_now()
  {
    // We have to terminate our children as well.
    terminate_jobs(children_);
    terminate_asap();
  }

  void
  Job::terminate_asap()
  {
    if (!terminated())
    {
      if (stats_.logging)
      {
        libport::utime_t start_resume = scheduler_.get_time();
        stats_.job.running.add_sample(
          start_resume - stats_.last_resume);
        // Just a precaution in case we end up in resume_scheduler_.
        stats_.last_resume = start_resume;
      }
      async_throw(TerminateException());
    }
  }

  void
  Job::terminate_child(const rJob& child)
  {
    child->terminate_now();
    libport::erase_if(children_, boost::lambda::_1 == child);
  }

  void
  Job::terminate_cleanup()
  {
    GD_FINFO_DEBUG("job %s: terminate_cleanup, considering %s jobs",
      this, to_wake_up_.size());
    // Wake-up waiting jobs.
    foreach (const rJob& job, to_wake_up_)
      if (!job->terminated())
      {
        GD_FINFO_DEBUG("job %s: waking up dependent job %s", this, *job);
	job->state_set(running);
      }
    to_wake_up_.clear();
    state_ = zombie;
    resume_scheduler_();
  }

  void
  Job::stats_type::thread_stats_type::add(const thread_stats_type& t)
  {
    running.add_samples(t.running);
    waiting.add_samples(t.waiting);
    sleeping.add_samples(t.sleeping);
    nb_fork += t.nb_fork;
    nb_join += t.nb_join;
    nb_exn += t.nb_exn;
  }

  void
  Job::copy_stats_to(rJob& b)
  {
    if (b && b->stats_.logging)
    {
      b->stats_.job.nb_join++;
      b->stats_.terminated_children.add(stats_.job);
    }
  }

  void
  Job::copy_stats_to_parent()
  {
    if (parent_)
      copy_stats_to(parent_);
  }

  void
  Job::register_child(const rJob& child, Collector& children)
  {
    aver(!child->parent_);
    child->parent_ = this;
    child->stats_.logging = stats_.logging;
    if (stats_.logging)
      stats_.job.nb_fork++;
    children.push_back(child);
    children_.push_back(child);
  }

  static bool
  job_compare(rJob lhs, rJob rhs)
  {
    return lhs == rhs;
  }

  void
  Job::yield_until_terminated(Job& other)
  {
    if (non_interruptible_ && this != &other)
      scheduling_error("dependency on other task in non-interruptible code");

    if (!other.terminated())
    {
      // We allow enqueuing on ourselves, but without doing it for real.
      if (&other != this)
	other.to_wake_up_.push_back(this);
      state_ = joining;
      try
      {
        GD_FINFO_DEBUG("job %s: queued to %s, waiting", this, &other);
        resume_scheduler_();
      }
      catch (...)
      {
        GD_FINFO_DEBUG("job %s: dequeued by exception", this);
        // We have been awoken by an exception; in this case,
        // dequeue ourselves from the other thread queue if
        // we are still enqueued there.
        libport::erase_if(other.to_wake_up_,
          boost::bind(job_compare, this, _1));
        throw;
      }
    }
  }

  void
  Job::yield_until_terminated(const jobs_type& jobs)
  {
    foreach (const rJob& job, jobs)
      yield_until_terminated(*job);
  }

  void
  Job::yield_until_things_changed()
  {
    if (non_interruptible_ && !frozen())
      scheduling_error("attempt to wait for condition changes"
                       " in non-interruptible code");

    state_ = waiting;
    resume_scheduler_();
  }

  void
  Job::async_throw(const exception& e, bool force_async)
  {
    if (stats_.logging)
      stats_.job.nb_exn++;

    // If this is the current job we are talking about, the exception
    // is synchronous.
    if (!force_async && scheduler_.is_current_job(this))
      e.rethrow();

    // Store the exception for later use.
    pending_exception_ = e.clone();

    // Now that we acquired an exception to raise, we are active again,
    // even if we were previously sleeping or waiting for something.
    if (state_ != to_start && state_ != zombie)
      state_ = running;
  }

  void
  Job::register_stopped_tag(const Tag& tag, const boost::any& payload)
  {
    size_t max_tag_check = (size_t)-1;
    if (exception* e = pending_exception_.get())
    {
      // If we are going to terminate, do nothing
      if (dynamic_cast<TerminateException*>(e))
	return;
      // If we already have a StopException stored, do not go any
      // further.
      if (StopException* se = dynamic_cast<StopException*>(e))
	max_tag_check = se->depth_get();
    }

    // Check if we are affected by this tag, up-to max_tag_check from
    // the beginning of the tag list.
    if (size_t pos = has_tag(tag, max_tag_check))
      async_throw(StopException(pos - 1, payload));
  }

  void
  Job::check_for_pending_exception()
  {
    // If an exception has been stored for further rethrow, now is
    // a good time to do so.
    if (has_pending_exception() && !ignore_pending_exceptions_get())
    {
      // Reset pending_exception_ by copying it into a local variable
      // first. This ensures that the exception will be destroyed
      // properly after having been rethrown.
      exception_ptr e = pending_exception_;
      e->rethrow();
    }
  }

  unsigned int
  Job::alive_jobs()
  {
    return alive_jobs_;
  }

  void
  Job::hook_preempted() const
  { /* nothing*/ }

  void
  Job::hook_resumed() const
  { /* nothing*/ }


  /*------------.
  | jobs_type.  |
  `------------*/

  void
  terminate_jobs(jobs_type& jobs)
  {
    foreach (rJob& job, jobs)
    {
      GD_FINFO_TRACE("Terminating %s", job);
      job->terminate_now();
    }
    jobs.clear();
  }

  std::ostream&
  operator<< (std::ostream& o, const jobs_type& js)
  {
    // Explicit parameters required by MSWC 2005.
    return o << libport::separate<const jobs_type,
                                  std::ostream&(*)(std::ostream&)>
                                 (js, libport::iendl);
  }


} // namespace sched
