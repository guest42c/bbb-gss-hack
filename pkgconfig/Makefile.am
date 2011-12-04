pcfiles = \
	gst-stream-server-@GST_MAJORMINOR@.pc

pcfiles_uninstalled = \
	gst-stream-server-@GST_MAJORMINOR@-uninstalled.pc

all-local: $(pcfiles) $(pcfiles_uninstalled)

### how to generate pc files
%-@GST_MAJORMINOR@.pc: %.pc
	cp $< $@
%-@GST_MAJORMINOR@-uninstalled.pc: %-uninstalled.pc
	cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = \
	gst-stream-server.pc.in \
	gst-stream-server.pc.in
CLEANFILES = $(pcfiles) $(pcfiles_uninstalled)

