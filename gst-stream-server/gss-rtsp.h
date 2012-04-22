
#ifndef _GSS_RTSP_H_
#define _GSS_RTSP_H_

#include "gss-server.h"

#include <gst/rtsp-server/rtsp-server.h>

G_BEGIN_DECLS


struct _GssRtspStream {

  GssServerStream *stream;
  GstRTSPServer *server;

  GstRTSPMediaMapping *mapping;
  GstRTSPMediaFactory *factory;
};

void gss_server_rtsp_init (GssServer *server);

GssRtspStream * gss_rtsp_stream_new (GssServerStream *stream);
void gss_rtsp_stream_free (GssRtspStream *rtsp_stream);
void gss_rtsp_stream_start (GssRtspStream *rtsp_stream);

G_END_DECLS

#endif


