
bin_PROGRAMS = ew-stream-server


ew_stream_server_CFLAGS = $(EWAPPS_CFLAGS) $(GST_CFLAGS) $(SOUP_CFLAGS) -Wall -Werror
ew_stream_server_LDADD = $(GST_LIBS) $(SOUP_LIBS)
ew_stream_server_SOURCES = \
	ew-hls-server.c \
	ew-hls-server.h \
	ew-stream-server.c \
	ew-server.c \
	ew-server.h \
	ew-config.c \
	ew-html.c \
	ew-html.h


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

