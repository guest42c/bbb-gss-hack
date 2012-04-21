
#ifndef _EW_STREAM_SERVER_H_
#define _EW_STREAM_SERVER_H_

#include <gst-stream-server/gss-server.h>

#define MAX_RATIO 0.9
#define MAX_STREAMS 50
#define LATENCY ((int)(10*GST_MSECOND))


void ew_stream_server_add_admin_callbacks (GssServer *server,
    SoupServer *soupserver);

#endif

