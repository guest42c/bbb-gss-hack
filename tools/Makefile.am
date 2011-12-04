
bin_PROGRAMS = \
	ew-stream-server \
	ew-stream-server2


ew_stream_server_CFLAGS = $(EWAPPS_CFLAGS) $(GST_CFLAGS) $(SOUP_CFLAGS) -Wall -Werror
ew_stream_server_LDADD = $(GST_LIBS) $(SOUP_LIBS) ../ew-stream-server/libess.la
ew_stream_server_SOURCES = ew-stream-server.c

ew_stream_server2_CFLAGS = $(EWAPPS_CFLAGS) $(GST_CFLAGS) $(SOUP_CFLAGS) -Wall -Werror
ew_stream_server2_LDADD = $(GST_LIBS) $(SOUP_LIBS) ../ew-stream-server/libess.la
ew_stream_server2_SOURCES = ew-stream-server2.c




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

