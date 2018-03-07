AC_PREREQ([2.69])

AC_INIT([xpacars], [0.1])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.13 foreign dist-bzip2 dist-xz dist-zip subdir-objects
                  serial-tests -Wall -Wno-extra-portability -Werror])
AC_CONFIG_HEADERS([config.h])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])

LT_INIT

AC_PROG_CC_C99
AS_IF([test "x$ac_cv_prog_cc_c99" = "xno"], [
  AC_MSG_ERROR([no C99 compiler found, xpacars requires a C99 compiler.])
])

bkp_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden"
AC_MSG_CHECKING([if ${CC} supports -fvisibility=hidden])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([char guda;])], [
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
  AC_MSG_ERROR([no -fvisibility=hidden support found in ${CC}])
])
CFLAGS="$bkp_CFLAGS"

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([LIBCURL], [libcurl])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AS_ECHO("
        ====== ${PACKAGE_STRING} ======

        prefix:              ${prefix}
        exec_prefix:         ${exec_prefix}
        bindir:              ${bindir}

        compiler:            ${CC}
        cflags:              ${CFLAGS}
        ldflags:             ${LDFLAGS}
")
