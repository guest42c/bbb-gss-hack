
bin_PROGRAMS = \
	ew-stream-server


ew_stream_server_CFLAGS = $(GSS_CFLAGS) $(GST_CFLAGS) $(SOUP_CFLAGS) $(GST_RTSP_SERVER_CFLAGS) $(JSON_GLIB_CFLAGS)
ew_stream_server_LDADD = $(GSS_LIBS) $(GST_LIBS) $(SOUP_LIBS) $(GST_RTSP_SERVER_LIBS) $(JSON_GLIB_LIBS)
ew_stream_server_SOURCES = \
	ew-stream-server.c \
	ew-stream-server.h \
	ew-config-stream-server.c





ew_prefix=/opt/entropywave/ew-media-sdk-linux-x86_64-2011.04/
appname=ew-stream-server-$(shell date +%Y%m%d)

app:
	rm -rf ew-stream-server-*
	mkdir $(appname)
	tar -xzf $(ew_prefix)/runtime/runtime.tgz --directory $(appname)
	cp ew-stream-server $(appname)/bin/ew-stream-server
	cp ${ew_prefix}/runtime/wrapper $(appname)/ew-stream-server
	cp ${ew_prefix}/runtime/wrapper $(appname)/gst-inspect-0.10
	cp ${ew_prefix}/runtime/wrapper $(appname)/gst-launch-0.10
	cp -ar ../ew-encoder/images $(appname)/
	tar -czf $(appname).tgz $(appname)

