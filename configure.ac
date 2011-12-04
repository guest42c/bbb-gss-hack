AC_PREREQ([2.58])
dnl PLEASE remove the nano when doing a release
AC_INIT(ew-stream-server,0.1.0.1)

AS_NANO([EWAPPS_CVS=no],[EWAPPS_CVS=yes])

AC_CANONICAL_HOST([])
AM_INIT_AUTOMAKE(1.6)

am__tar='tar chf - "$$tardir"'

# make libtool behave on windows
lt_cv_deplibs_check_method=pass_all

EWAPPS_MAJORMINOR=1.0
AC_SUBST(EWAPPS_MAJORMINOR)

AM_CONFIG_HEADER(config.h)

AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AM_PROG_AS
AC_PROG_CXX
AM_PROG_CC_C_O
AC_SYS_LARGEFILE
LT_PATH_NM

dnl allow for different autotools
AS_AUTOTOOLS_ALTERNATE

AC_CONFIG_SRCDIR([ew-stream-server/ew-server.c])

AC_CONFIG_MACRO_DIR([m4])
ACLOCAL_AMFLAGS="-I m4 $ACLOCAL_AMFLAGS"
AC_SUBST(ACLOCAL_AMFLAGS)

dnl set up gettext
dnl the version check needs to stay here because autopoint greps for it
#AM_GNU_GETTEXT_VERSION([0.11.5])
#AM_GNU_GETTEXT([external])

AC_CHECK_LIBM
AC_SUBST(LIBM)

AS_COMPILER_FLAG(-Wall, EWAPPS_CFLAGS="$EWAPPS_CFLAGS -Wall")
if test "x$EWAPPS_CVS" = "xyes"
then
  AS_COMPILER_FLAG(-Werror, EWAPPS_CFLAGS="$EWAPPS_CFLAGS -Werror")
fi

#AS_COMPILER_FLAG(-O3, CFLAGS=`echo $CFLAGS | sed s/O2/O3/`)
#AS_COMPILER_FLAG(-O3, CXXFLAGS=`echo $CXXFLAGS | sed s/O2/O3/`)

# Enable severe pain
#AS_COMPILER_FLAGS(COMPANY_CFLAGS,"-Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Wold-style-definition -Wdeclaration-after-statement -Wmissing-declarations -Wmissing-prototypes -Wredundant-decls -Wmissing-noreturn -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Winline -Wformat-nonliteral -Wformat-security -Wswitch-enum -Wswitch-default")
#COMPANY_CFLAGS="-Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Wold-style-definition -Wdeclaration-after-statement -Wmissing-declarations -Wmissing-prototypes -Wredundant-decls -Wmissing-noreturn -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Winline -Wformat-nonliteral -Wformat-security -Wswitch-enum -Wswitch-default"
#EWAPPS_CFLAGS="$EWAPPS_CFLAGS $COMPANY_CFLAGS"

EWAPPS_CFLAGS="$EWAPPS_CFLAGS -I\$(top_srcdir)"
AC_SUBST(EWAPPS_CFLAGS)

########################
# GStreamer stuff
########################

GST_MAJORMINOR=0.10

dnl *** check for arguments to configure ***

PKG_CHECK_MODULES(GST, gstreamer-0.10 gstreamer-base-0.10,
    HAVE_GST=yes, HAVE_GST=no)
PKG_CHECK_MODULES(GST_VIDEO, gstreamer-video-0.10 >= 0.10.18,
    HAVE_GST_VIDEO=yes, HAVE_GST_VIDEO=no)
PKG_CHECK_MODULES(GST_AUDIO, gstreamer-audio-0.10 >= 0.10.18,
    HAVE_GST_AUDIO=yes, HAVE_GST_AUDIO=no)
PKG_CHECK_MODULES(GTK, gtk+-2.0 gmodule-export-2.0,
    HAVE_GTK=yes, HAVE_GTK=no)
PKG_CHECK_MODULES(SOUP, libsoup-2.4, HAVE_SOUP=yes, HAVE_SOUP=no)
if test "$HAVE_GST" != yes -o "$HAVE_GTK" != yes -o "$HAVE_GST_VIDEO" != yes -o "$HAVE_GST_AUDIO" != yes ; then
  echo GStreamer and Gtk+ are needed
  exit 1
fi

AM_CONDITIONAL(HAVE_GST, test "x$HAVE_GST" = "xyes")
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

GST_ALL_LDFLAGS="-no-undefined"
AC_SUBST(GST_ALL_LDFLAGS)

PKG_CHECK_MODULES(LIBICAL, libical, HAVE_LIBICAL=yes, HAVE_LIBICAL=no)
AM_CONDITIONAL(HAVE_LIBICAL, test "x$HAVE_LIBICAL" = "xyes")
AC_SUBST(LIBICAL_CFLAGS)
AC_SUBST(LIBICAL_LIBS)
AC_DEFINE(HAVE_LIBICAL, 1, [Defined if libical was found])

PKG_CHECK_MODULES(CAIRO, cairo, HAVE_CAIRO=yes, HAVE_CAIRO=no)
AM_CONDITIONAL(HAVE_CAIRO, test "x$HAVE_CAIRO" = "xyes")
AC_SUBST(CAIRO_CFLAGS)
AC_SUBST(CAIRO_LIBS)

AM_CONDITIONAL(HAVE_FALSE, false)

AS_HOST_DEFINES()

AC_CONFIG_FILES([
Makefile 
ew-stream-server/Makefile
tools/Makefile
])
AC_OUTPUT

#case "$host_os" in
#  mingw32*)
#    sed 's/^deplibs_check_method=.*/deplibs_check_method="pass_all"/' libtool >libtool.tmp && mv libtool.tmp libtool
#    ;;
#esac

