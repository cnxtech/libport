#                                               -*- autoconf -*-
# urbi-libtool.m4: This file is part of build-aux.
# Copyright (C) Gostai S.A.S., 2008.
#
# This software is provided "as is" without warranty of any kind,
# either expressed or implied, including but not limited to the
# implied warranties of fitness for a particular purpose.
#
# See the LICENSE file for more information.
# For comments, bug reports and feedback: http://www.urbiforge.com
#

AC_DEFUN([URBI_LIBTOOL],
[
if test $ac_cv_cxx_compiler_ms = yes; then
# It is perfectly possible to link with static libraries when building dlls.
# Also for some reason archive_cmds ends up empty for C++ tag.
 AC_CONFIG_COMMANDS([libtool-quirk],
     [sed -i -r -e 's/^archive_cmds=""//' \
     -e 's/^deplibs_check_method=.*/deplibs_check_method=pass_all/' libtool])
 fi
])
