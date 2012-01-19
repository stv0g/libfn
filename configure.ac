#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT(libfn, 0.1, info@steffenvogel.de)
AM_INIT_AUTOMAKE(libfn, 0.1)
LT_INIT
AC_CONFIG_SRCDIR([src/libfn.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([m], [abs])
PKG_CHECK_MODULES([FNVUM_DEPS], [sdl >= 1.2.14 fftw3 >= 3.2.2 libpulse-simple >= 0.9.21])
PKG_CHECK_MODULES([FNPOM_DEPS], [json >= 0.9])
PKG_CHECK_MODULES([FNWEB_DEPS], [json >= 0.9 libmicrohttpd >= 0.4.6])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset pow socket sqrt strrchr atoi])

AC_CONFIG_FILES([Makefile src/Makefile fn.pc])
AC_OUTPUT
