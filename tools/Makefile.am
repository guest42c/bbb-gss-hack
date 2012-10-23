
bin_PROGRAMS = \
	ew-stream-server

noinst_PROGRAMS = \
	vts-server


ew_stream_server_CFLAGS = $(GSS_CFLAGS) $(GST_CFLAGS) $(SOUP_CFLAGS) $(GST_RTSP_SERVER_CFLAGS) $(JSON_GLIB_CFLAGS)
ew_stream_server_LDADD = $(GSS_LIBS) $(GST_LIBS) $(SOUP_LIBS) $(GST_RTSP_SERVER_LIBS) $(JSON_GLIB_LIBS)
ew_stream_server_SOURCES = \
	ew-stream-server.c \
	ew-stream-server.h

vts_server_CFLAGS = $(GSS_CFLAGS) $(GST_CFLAGS) $(SOUP_CFLAGS) $(GST_RTSP_SERVER_CFLAGS) $(JSON_GLIB_CFLAGS)
vts_server_LDADD = $(GSS_LIBS) $(GST_LIBS) $(SOUP_LIBS) $(GST_RTSP_SERVER_LIBS) $(JSON_GLIB_LIBS)
vts_server_SOURCES = \
	vts-server.c



