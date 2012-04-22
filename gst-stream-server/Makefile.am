
lib_LTLIBRARIES = libgss-@GST_MAJORMINOR@.la

gss_includedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gss

libgss_@GST_MAJORMINOR@_la_CFLAGS = $(GSS_CFLAGS) $(GST_CFLAGS) $(SOUP_CFLAGS)
libgss_@GST_MAJORMINOR@_la_LIBS = $(GST_LIBS) $(SOUP_LIBS)
libgss_@GST_MAJORMINOR@_la_SOURCES = \
	gss-hls-server.c \
	gss-server.c \
	gss-session.c \
	gss-config.c \
	gss-form.c \
	gss-html.c \
	gss-soup.c \
	gss-rtsp.c

gss_include_HEADERS = \
	gss-hls-server.h \
	gss-server.h \
	gss-session.h \
	gss-config.h \
	gss-form.h \
	gss-html.h \
	gss-soup.h \
	gss-rtsp.h


