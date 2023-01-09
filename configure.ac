#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libmalloc], [pre-alpha])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_CONFIG_SRCDIR([src/bitmap_alloc.c])
AC_CONFIG_HEADERS([include/config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_AS 
AM_PROG_AR
AC_PROG_RANLIB

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE

AC_ARG_ENABLE([tests], [Compile test programs], [tests=true])
AM_CONDITIONAL([BUILD_TESTS], [test x$tests = xtrue])

AC_CONFIG_FILES([Makefile src/Makefile include/Makefile tests/Makefile])
AC_OUTPUT
