namespace scheduler
{

  inline
  Tag::Tag (libport::Symbol name)
    : parent_ (0),
      blocked_ (false),
      frozen_ (false),
      name_ (name)
  {
  }

  inline
  Tag::~Tag ()
  {
  }

  inline rTag
  Tag::fresh (libport::Symbol name)
  {
    rTag res = new Tag (name);
    res->self_ = res;
    return res;
  }

  inline rTag
  Tag::fresh (rTag parent, libport::Symbol name)
  {
    rTag res = new Tag (parent, name);
    res->self_ = res;
    return res;
  }

  inline bool
  Tag::frozen () const
  {
    return frozen_ || (parent_ && parent_->frozen ());
  }

  inline bool
  Tag::blocked () const
  {
    return blocked_ || (parent_ && parent_->blocked ());
  }

  inline void
  Tag::freeze (const Job&)
  {
    frozen_ = true;
  }

  inline void
  Tag::unfreeze (const Job&)
  {
    frozen_ = false;
  }

  inline void
  Tag::block (const Job& job)
  {
    blocked_ = true;
    stop (job);
  }

  inline void
  Tag::unblock (const Job&)
  {
    blocked_ = false;
  }

  inline bool
  Tag::own_blocked () const
  {
    return blocked_;
  }

  inline void
  Tag::set_blocked (bool b)
  {
    blocked_ = b;
  }

  inline rTag
  Tag::self () const
  {
    rTag res;
    res = self_.lock ();
    return res;
  }

  inline const libport::Symbol&
  Tag::name_get () const
  {
    return name_;
  }

} // namespace scheduler
