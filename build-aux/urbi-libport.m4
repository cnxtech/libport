dnl
dnl urbi-libport.m4: This file is part of build-aux.
dnl Copyright (C) Gostai S.A.S., 2006-2008.
dnl
dnl This software is provided "as is" without warranty of any kind,
dnl either expressed or implied, including but not limited to the
dnl implied warranties of fitness for a particular purpose.
dnl
dnl See the LICENSE file for more information.
dnl For comments, bug reports and feedback: http://www.urbiforge.com
dnl

# _URBI_LIBPORT_COMMON
# --------------------
# Code common to the use of an installed or shipped libport.
AC_DEFUN([_URBI_LIBPORT_COMMON],
[
AC_ARG_WITH([boost-thread-static],
	[AC_HELP_STRING([--with-boost-thread-static]
			[link with the static version of boost thread library])]
	[], [with_boost_thread_static=yes])

AC_REQUIRE([URBI_PTHREAD])dnl
AC_REQUIRE([URBI_FLOAT_CHECK])dnl

AC_CHECK_HEADERS([sched.h sys/resource.h sys/mman.h])
AC_CHECK_FUNCS([sched_setscheduler setpriority mlockall])
# Check for Boost headers
BOOST_REQUIRE([1.34])
# Check for Boost.Thread
if test -n "$openr" && $openr; then
  AC_MSG_NOTICE([[OpenR in use: not using Boost.Thread.]])
else
  if test x$with_boost_thread_static = xyes; then
    boost_threads_flag=s
  fi
  BOOST_THREADS([$boost_threads_flag])
fi
])


# URBI_LIBPORT_INSTALLED
# ----------------------
# We use an installed libport, most probably that of the kernel we
# use.  We don't run URBI_UFLOAT since, of course, we use the same
# kind of ufloat as the kernel does.
#
# Cores use an installed libport (in kernelincludedir), *and* install it
# (in sdkincludedir).
AC_DEFUN([URBI_LIBPORT_INSTALLED],
[AC_REQUIRE([_URBI_LIBPORT_COMMON])dnl
AC_AFTER([$0], [URBI_DIRS])dnl

# Check that we can find libport files.
libport_config_hh=$(URBI_RESOLVE_DIR([$kernelincludedir/libport/config.h]))
if test ! -f $libport_config_h; then
  AC_ERROR([--with-urbi-kernel: cannot find $libport_config_h])
fi
])

# URBI_LIBPORT
# ------------
# Invoke the macros needed by a shipped URBI_LIBPORT (i.e., a non-installed
# copy in this source tree).
AC_DEFUN([URBI_LIBPORT],
[AC_REQUIRE([_URBI_LIBPORT_COMMON])dnl
AC_REQUIRE([URBI_UFLOAT])dnl

# Create libport/config.h.
URBI_PREFIX_CONFIG_H([include/libport/config.h], [LIBPORT])

# $(top_srcdir) to find sources, $(top_builddir) to find libport/config.h.
AC_SUBST([LIBPORT_CPPFLAGS],
	 ['-I$(top_srcdir)/include -I$(top_builddir)/include'])

# Where we install the libport files (not including the /libport suffix).
URBI_PACKAGE_KIND_SWITCH(
  [kernel],     [AC_SUBST([libport_include_basedir], ['${kernelincludedir}'])],
  [sdk],	[AC_SUBST([libport_include_basedir], ['${includedir}'])],
  [core],	[AC_SUBST([libport_include_basedir], ['${sdkincludedir}'])])
])

## Local Variables:
## mode: autoconf
## End:
