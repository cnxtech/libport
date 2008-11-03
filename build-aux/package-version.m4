#
# urbi-package-version.m4: This file is part of build-aux.
# Copyright (C) 2006-2008, Gostai S.A.S.
#
# This software is provided "as is" without warranty of any kind,
# either expressed or implied, including but not limited to the
# implied warranties of fitness for a particular purpose.
#
# See the LICENSE file for more information.
# For comments, bug reports and feedback: http://www.urbiforge.com
#

m4_pattern_forbid([^URBI_])

AC_PREREQ([2.60])

# URBI_PACKAGE_VERSION
# --------------------
# Use either .tarball-version or .version to recover the current
# version info, in order to avoid Autoconf-time dependencies on the
# version number.
AC_DEFUN([URBI_PACKAGE_VERSION],
[AC_MSG_CHECKING([for current version])
while true
do
  # If the version is already defined, do nothing.
  test "$VERSION" != UNDEFINED ||
    break
  for ac_f in .tarball-version .version
  do
    if test -f $srcdir/$ac_f; then
      VERSION=$(cat $srcdir/$ac_f) ||
        AC_MSG_ERROR([cannot read $srcdir/$ac_f])
      test -n "$VERSION" ||
        AC_MSG_ERROR([cannot $srcdir/$ac_f is empty])
      # Found it.
      break 2
    fi
  done
  # Nothing found, try to get it from the the repo.
  VERSION=$(cd $srcdir && build-aux/git-version-gen --file)
  break
done
test -n "$VERSION" ||
  AC_MSG_ERROR([cannot get version information])
AC_MSG_RESULT([$VERSION])
])

## Local Variables:
## mode: autoconf
## End: