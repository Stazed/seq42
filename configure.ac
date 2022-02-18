dnl Process this file with autoconf to produce a configure script.
AC_INIT([seq42],[1.1.5],[stazed@mapson.com])

AC_CONFIG_SRCDIR([src/event.cpp])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])

AM_INIT_AUTOMAKE([dist-bzip2])

AC_DEFINE(_GNU_SOURCE, 1, [gnu source])

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AX_CXX_COMPILE_STDCXX_11(noext,mandatory)

dnl Do we have -Wl,--as-needed?
AC_MSG_CHECKING(if --as-needed works)
AC_ARG_ENABLE(as_needed,[ --enable-as-needed enable -Wl,--as-needed],
     [
     case "${enableval}" in
         yes) as_needed="1";;
         no) as_needed="";;
         *) AC_MSG_ERROR(bad value ${enableval} for --enable-as_needed);;
     esac
     ],[
         as_needed="unknown"
     ])
if test x"${as_needed}" = x"unknown"; then
    ac_old_ldflags="${LDFLAGS}"
    LDFLAGS="-Wl,--as-needed"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[as_needed="1"],[as_needed=""])
    LDFLAGS="${ac_old_ldflags}" 
fi
if test -n "$as_needed"; then
    AC_MSG_RESULT(yes)
    LDFLAGS="${LDFLAGS} -Wl,--as-needed"
else
    AC_MSG_RESULT(no)
fi

dnl Checks for libraries.
AC_CHECK_LIB(rt, main, ,    
	AC_MSG_ERROR([POSIX.1b RealTime Library Missing -lrt]))
AC_CHECK_LIB(gtkmm-3.0, _init,,
             AC_MSG_ERROR(Essential library libgtkmm-3.0 not found))
AC_CHECK_LIB(sigc-2.0, main,,
             AC_MSG_ERROR(Essential library libsigc++-2.0 not found))
AC_CHECK_LIB(asound, snd_pcm_open,,
             AC_MSG_ERROR(Essential library asound not found))

dnl Checks for header files.
AC_CHECK_HEADERS(unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

dnl Checks for library functions.
AC_CHECK_FUNCS(select)

AM_PATH_ALSA(0.9.0)

PKG_CHECK_MODULES(GTKMM, gtkmm-3.0 >= 3.0.0)
AC_SUBST(GTKMM_CFLAGS)
AC_SUBST(GTKMM_LIBS)


dnl
dnl JACK SUPPORT
dnl
AC_ARG_ENABLE(jack,
    [AS_HELP_STRING(--disable-jack, [Disable JACK support])],
    [jack=$enableval],
    [jack=yes])

if test "$jack" != "no"; then
    PKG_CHECK_MODULES(JACK, jack >= 0.90.0, jack_found="yes", jack_found="no")
    if test "$jack_found" = "yes"; then
        AC_DEFINE(JACK_SUPPORT, 1, [Define to enable JACK driver])
        AC_SUBST(JACK_CFLAGS)
        AC_SUBST(JACK_LIBS)
    else
        AC_MSG_WARN([*** Could not find JACK library, disabling JACK support])
    fi
else
    AC_MSG_WARN([*** JACK support not enabled.]);
fi


dnl
dnl    MIDI CONTROL SUPPORT
dnl
midi_control="yes"

AC_ARG_ENABLE(midi_control, [AS_HELP_STRING(--disable-midi-control, [Disable MIDI transport control support])], [midi_control=$enableval])

if test "$midi_control" = "yes"; then
		AC_DEFINE(MIDI_CONTROL_SUPPORT, 1, [Define to enable MIDI transport control support])
else
    AC_MSG_WARN([ *** MIDI control support not enabled.]);
fi

dnl Enable NSM support.

AC_ARG_ENABLE(nsm,
  AS_HELP_STRING([--enable-nsm], [Enable NSM support (default=yes)]),
  [ac_nsm=$enableval],
  [ac_nsm=yes])

dnl Check for NSM support.

if test "$ac_nsm" != "no"; then
    AC_DEFINE(NSM_SUPPORT, 1, [Define if NSM support is available.])
    AC_DEFINE(NSM_SUPPORT, 1, [Define to enable NSM])
    AC_MSG_RESULT([Sessions/NSM support enabled.]);
fi

dnl Check for now-required LIBLO library, needed for NSM support.

PKG_CHECK_MODULES([LIBLO], [liblo], [ac_liblo="yes"], [ac_liblo="no"])
if test "x$ac_liblo" = "xyes"; then
    AC_DEFINE(LIBLO_SUPPORT, 1, [Define if LIBLO library is available.])
    AC_SUBST(LIBLO_CFLAGS)
    AC_SUBST(LIBLO_LIBS)
    AC_MSG_RESULT([LIBLO support enabled.]);
else
    AC_MSG_ERROR([LIBLO library not found, OSC dev package required.])
    ac_nsm="no"
fi


AC_CONFIG_FILES([Makefile man/Makefile icons/Makefile src/Makefile src/pixmaps/Makefile])
AC_OUTPUT

