# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([hwcnt], [0.01], [michael.laurenzano@gmail.com])
AC_CONFIG_SRCDIR([include/hwcnt.hpp])

AC_PROG_CC
AC_PROG_CXX

###############################################################
## find PAPI lib/include
###############################################################
PAPI_LIB_EXT="so"
AC_ARG_ENABLE(papi-shared, [AS_HELP_STRING([--disable-papi-shared],[use papi static archives instead of shared libs])], [papishared=$enableval], [papishared=yes])
if test "$papishared" = "yes"; then
   PAPI_LIB_EXT="so"
else
   PAPI_LIB_EXT="a"
fi

PAPI_INCLUDE=""
PAPI_LIB_PATH=""
PAPI_LD_FLAGS=""
PAPI_BIN_PATH=""
AC_ARG_WITH(papi,
            [AS_HELP_STRING([--with-papi=<path>],[path to papi])],
            [papilib=$withval
             AC_CHECK_HEADER([$papilib/include/papi.h],[PAPI_INCLUDE="-I$papilib/include"
                                PAPI_BIN_PATH="$papilib/bin"
                                PAPI_LIB_PATH="$papilib/lib"],
                             [AC_MSG_ERROR($papilib/include/papi.h not found)])],)

AC_ARG_WITH(papi-include,
            [AS_HELP_STRING([--with-papi-include=<path>],[path to papi header (papi.h)])],
            [papiinc=$withval
             AC_CHECK_HEADER([$papiinc/papi.h],[PAPI_INCLUDE="-I$papiinc"],
            [AC_MSG_ERROR($papiinc/papi.h not found)])],)

AC_ARG_WITH(papi-lib,
            [AS_HELP_STRING([--with-papi-lib=<path>],[path to papi library (libpapi.$PAPI_LIB_EXT)])],
            [papilib=$withval
             AC_CHECK_FILE([$papilib/libpapi.$PAPI_LIB_EXT],[PAPI_LIB_PATH="$papilib"],
                           [AC_MSG_ERROR($papilib/libpapi.$PAPI_LIB_EXT not found)])],)

AC_ARG_WITH(papi-bin,
            [AS_HELP_STRING([--with-papi-bin=<path>],[path to papi binaries (e.g., papi_avail)])],
            [papibin=$withval
             AC_CHECK_FILES([$papibin/papi_avail
                             $papibin/papi_native_avail
                             $papibin/papi_event_chooser],[PAPI_BIN_PATH="$papibin"],
                           [AC_MSG_ERROR(one of the papi binaries (papi_avail papi_native_avail papi_event_chooser) not found in $papibin)])],)

if test "$PAPI_INCLUDE" = ""; then
   AC_CHECK_HEADER([papi.h],[PAPI_INCLUDE=""],[AC_MSG_ERROR(papi.h not found, consider using --with-papi or --with-papi-include)])
fi
if test "$PAPI_LIB_PATH" = ""; then
   AC_CHECK_LIB([papi],[PAPI_library_init],[PAPI_LIB_PATH=""],[AC_MSG_ERROR(libpapi not found, consider using --with-papi or --with-papi-lib)])
fi
if test "$PAPI_BIN_PATH" = ""; then
   AC_PATH_PROG([avail], [papi_avail], ["no"])
   if test "$avail" = "no"; then
      AC_MSG_ERROR([cannot find papi_avail in PATH, consider using --with-papi or --with-papi-bin])
   fi
   AC_PATH_PROG([native_avail], [papi_native_avail], ["no"])
   if test "$native_avail" = "no"; then
      AC_MSG_ERROR([cannot find papi_native_avail in PATH, consider using --with-papi or --with-papi-bin])
   fi
   AC_PATH_PROG([event_chooser], [papi_event_chooser], ["no"])
   if test "$event_chooser" = "no"; then
      AC_MSG_ERROR([cannot find papi_event_chooser in PATH, consider using --with-papi or --with-papi-bin])
   fi
fi

PAPI_LD_TMP=""
if test "$PAPI_LIB_PATH" = ""; then
   PAPI_LD_TMP="-lpapi"
else
   PAPI_LD_TMP="-L$PAPI_LIB_PATH -lpapi"
fi

if test "$PAPI_LIB_EXT" = "a"; then
   PAPI_LD_FLAGS="-Wl,-Bstatic $PAPI_LD_TMP -Wl,-Bdynamic"
else
   PAPI_LD_FLAGS="$PAPI_LD_TMP"
fi

AC_SUBST(PAPI_INCLUDE)
AC_SUBST(PAPI_LIB_PATH)
AC_SUBST(PAPI_LD_FLAGS)
AC_SUBST(PAPI_BIN_PATH)
###############################################################
###############################################################

AC_ARG_ENABLE(cltools, [AS_HELP_STRING([--disable-cltools],[don't build command line tools])], [cltools=$enableval], [cltools=yes])
CLTOOLS_DEFS=""
if test "$cltools" = "yes"; then
   AC_CHECK_HEADER([getopt.h],, [AC_MSG_ERROR([Cannot find getopt.h, which is required for the command line tools. use --disable-cltools to proceed anyway])])
else
   CLTOOLS_DEFS="-DDISABLE_CLTOOLS"
fi

AC_SUBST(CLTOOLS_DEFS)

AC_CONFIG_FILES([src/Makefile
                examples/Makefile
                env/bashrc])
AC_OUTPUT
