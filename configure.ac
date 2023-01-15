#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_COPYRIGHT([Copyright (C) 2016-2021 Robert Cunnings, NW8L])

AC_PREREQ([2.69])
AC_INIT([arim], [2.12], [nw8l@whitemesa.com])
AC_CONFIG_SRCDIR([src/ui.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CANONICAL_TARGET

# Checks for programs.
: ${CFLAGS=""}
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS([pthread_create], [pthread], [have_pthread=true], [have_pthread=false])
if test "x$have_pthread" != xtrue; then
    AC_MSG_ERROR(pthread library not found; ARIM cannot be built!)
fi
AC_SEARCH_LIBS([inflate], [z], [have_zlib=true], [have_zlib=false])
if test "x$have_zlib" != xtrue; then
    AC_MSG_ERROR(z compression library not found; ARIM cannot be built!)
fi
AC_SEARCH_LIBS([initscr], [ncurses], [have_ncurses=true], [have_ncurses=false])
if test "x$have_ncurses" != xtrue; then
    AC_MSG_ERROR(ncurses library not found; ARIM cannot be built!)
fi

AC_SEARCH_LIBS([rig_open], [hamlib], [have_hamlib=true], [have_hamlib=false])
if test "x$have_hamlib" != xtrue; then
    AC_MSG_ERROR(hamlib library not found; ARIM cannot be built!)
fi

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for package options.
AC_ARG_ENABLE([portable-bin],
    [AS_HELP_STRING([--enable-portable-bin], [enable portable binary package build])],
    [case "${enableval}" in
       yes) portable_bin=true ;;
       no)  portable_bin=false ;;
       *) AC_MSG_ERROR([bad value ${enableval} for --enable-portable-bin]) ;;
     esac],[portable_bin=false])
AM_CONDITIONAL([PORTABLE_BIN], [test "x$portable_bin" = xtrue])
AM_COND_IF([PORTABLE_BIN],
    AC_PREFIX_DEFAULT(${HOME}/${PACKAGE_NAME}-${PACKAGE_VERSION}))
AM_COND_IF([PORTABLE_BIN],
    AC_DEFINE([PORTABLE_BIN], [1], [Enable portable binary build]))

AM_CONDITIONAL([HAVE_NCURSES], [test "x$have_ncurses" = xtrue])
AM_COND_IF([HAVE_NCURSES],
    AC_DEFINE([HAVE_NCURSES], [1], [ncurses library found]))

AM_CONDITIONAL([HAVE_HAMLIB], [test "x$have_hamlib" = xtrue])
AM_COND_IF([HAVE_HAMLIB],
    AC_DEFINE([HAVE_HAMLIB], [1], [hamlib library found]))

AM_CONDITIONAL([HAVE_PTHREAD], [test "x$have_pthread" = xtrue])
AM_COND_IF([HAVE_PTHREAD],
    AC_DEFINE([HAVE_PTHREAD], [1], [pthread library found]))

AM_CONDITIONAL([HAVE_ZLIB], [test "x$have_zlib" = xtrue])
AM_COND_IF([HAVE_ZLIB],
    AC_DEFINE([HAVE_ZLIB], [1], [zlib library found]))

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_C_INLINE
AC_C_BIGENDIAN(
  [little_endian=false], [little_endian=true]
)
AM_CONDITIONAL([NATIVE_LITTLE_ENDIAN], [test "x$little_endian" = xtrue])
AM_COND_IF([NATIVE_LITTLE_ENDIAN],
    AC_DEFINE([NATIVE_LITTLE_ENDIAN], [1], [little endian target]))

# Checks for library functions.
AC_FUNC_MKTIME
AC_CHECK_FUNCS([gettimeofday memmove memset mkdir select socket strcasecmp strncasecmp strstr snprintf])

AM_INIT_AUTOMAKE([-Wall, subdir-objects])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

# print summary
AC_MSG_RESULT([
Configuration summary:

  Version .............................. $VERSION

  Target OS ............................ $target_os-$target_cpu

  Enable portable build ................ $portable_bin

  ncurses library found ................ $have_ncurses
  hamlib library found ................. $have_hamlib
  pthread library found ................ $have_pthread
  zlib library found ................... $have_zlib

  Installation prefix .................. $prefix

])

