pcfiles = \
	gst-streaming-server-@GST_MAJORMINOR@.pc

pcfiles_uninstalled = \
	gst-streaming-server-@GST_MAJORMINOR@-uninstalled.pc

all-local: $(pcfiles) $(pcfiles_uninstalled)

### how to generate pc files
%-@GST_MAJORMINOR@.pc: %.pc
	cp $< $@
%-@GST_MAJORMINOR@-uninstalled.pc: %-uninstalled.pc
	cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = \
	gst-streaming-server.pc.in \
	gst-streaming-server.pc.in
CLEANFILES = $(pcfiles) $(pcfiles_uninstalled)

