
dnl This file is part of diff-pdf.
dnl
dnl Copyright (C) 2009 TT-Solutions.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.


AC_PREREQ(2.61)
AC_INIT(diff-pdf, 0.5.2, [vaclav@slavik.io])

AC_CONFIG_SRCDIR([diff-pdf.cpp])
AC_CONFIG_AUX_DIR([admin])

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl remember, "build" is where we compile, "host" is where the resulting
dnl program runs (which may be different from "build" for cross-compilation)
AC_CANONICAL_HOST


dnl === Program checks ===

AC_PROG_CXX
AC_LANG(C++)

dnl === Library checks ===

PKG_CHECK_MODULES(POPPLER,
                  [glib-2.0 >= 2.36 poppler-glib >= 0.10 cairo-pdf])

AM_OPTIONS_WXCONFIG
AM_PATH_WXCONFIG([3.0.0], [wxfound=1], [wxfound=0], [core,base])
if test "$wxfound" != 1 ; then
    AC_MSG_ERROR([wxWidgets is required])
fi

dnl === Generate output files ===

AC_CONFIG_FILES([
    Makefile
])
AC_OUTPUT
