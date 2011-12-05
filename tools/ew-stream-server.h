
#ifndef _EW_STREAM_SERVER2_H_
#define _EW_STREAM_SERVER2_H_

#include <gst/gst.h>
#include <gst/interfaces/tuner.h>
#include <gst/interfaces/colorbalance.h>

#include <gst-stream-server/gss-server.h>

#define MAX_RATIO 0.9
#define MAX_STREAMS 50
#define LATENCY ((int)(10*GST_MSECOND))

typedef struct _EwEncoder EwEncoder;
typedef struct _EwStream EwStream;

struct _EwStream {
  int index;
  gboolean enabled;
  char *desc;

  int video_codec;
  int video_bitrate;
  int video_buffer_size;
  int video_width;
  int video_height;
  int par_n;
  int par_d;
  int dest;

  int audio_codec;
  int audio_bitrate;
  int muxer;

  char *host;
  int port;

  int default_speed;
  int fast_speed;

  double quality;
  int keyframe_distance;
  char *shoutcast_password;
  char *shoutcast_stream;

  EwServerStream *server_stream;

  GstElement *venc;
  GstElement *vscale;
  GstElement *aenc;
  GstElement *mux;
  GstElement *pre_venc_queue;
  GstElement *post_venc_queue;
  GstElement *pre_aenc_queue;
  GstElement *post_aenc_queue;
  GstElement *output_queue;
  GstElement *sink;
  GstElement *httpsink;
  GstElement *tagsetter;
  guint64 bytes_sent;
};


#endif

