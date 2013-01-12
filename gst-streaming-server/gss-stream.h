/* GStreamer Streaming Server
 * Copyright (C) 2009-2012 Entropy Wave Inc <info@entropywave.com>
 * Copyright (C) 2009-2012 David Schleef <ds@schleef.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifndef _GSS_STREAM_H
#define _GSS_STREAM_H

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#ifdef ENABLE_RTSP
#include <gst/rtsp-server/rtsp-server.h>
#endif
#include <libsoup/soup.h>
#include "gss-config.h"
#include "gss-types.h"
#include "gss-object.h"
#include "gss-session.h"

G_BEGIN_DECLS

#define GSS_TYPE_STREAM \
  (gss_stream_get_type())
#define GSS_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GSS_TYPE_STREAM,GssStream))
#define GSS_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GSS_TYPE_STREAM,GssStreamClass))
#define GSS_IS_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GSS_TYPE_STREAM))
#define GSS_IS_STREAM_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GSS_TYPE_STREAM))


#define GSS_STREAM_HLS_CHUNKS 20

typedef enum {
  GSS_STREAM_TYPE_UNKNOWN,
  GSS_STREAM_TYPE_OGG_THEORA_VORBIS,
  GSS_STREAM_TYPE_WEBM,
  GSS_STREAM_TYPE_M2TS_H264BASE_AAC,
  GSS_STREAM_TYPE_M2TS_H264MAIN_AAC,
  GSS_STREAM_TYPE_FLV_H264BASE_AAC,
  GSS_STREAM_TYPE_OGG_THEORA_OPUS
} GssStreamType;

struct _GssHLSSegment {
  int index;
  SoupBuffer *buffer;
  char *location;
  int duration;
};

struct _GssStream {
  GssObject object;

  /* properties */
  GssStreamType type;
  int width;
  int height;
  int bitrate;

  GssProgram *program;
  GssMetrics *metrics;

  char *codecs;
  char *playlist_location;
  char *location;

  /* GStreamer */
  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  int program_id;
  gboolean is_hls;

  /* for follow programs */
  char *follow_url;

  GssResource *resource;
  GssResource *playlist_resource;

  /* HLS */
  GstAdapter *adapter;
  int n_chunks;
  GssHLSSegment chunks[GSS_STREAM_HLS_CHUNKS];
  struct {
    gboolean need_index_update;
    SoupBuffer *index_buffer; /* contents of current index file */

    gboolean at_eos; /* true if sliding window is at the end of the stream */
  } hls;

  /* RTSP */
#ifdef ENABLE_RTSP
  GssRtspStream *rtsp_stream;
#else
  void *rtsp_stream;
#endif
};

typedef struct _GssStreamClass GssStreamClass;
struct _GssStreamClass {
  GssObjectClass object_class;

};

struct _GssConnection {
  SoupMessage *msg;
  SoupClientContext *client;
  GssStream *stream;
  GssProgram *program;
};


/* internal */
#define GSS_STREAM_MAX_FDS 65536
typedef struct _FDInfo FDInfo;
struct _FDInfo {
  void (*callback) (GssStream *stream, int fd, void *priv);
  void *priv;
};
FDInfo gss_stream_fd_table[GSS_STREAM_MAX_FDS];
/* end internal */


GType gss_stream_get_type (void);
GType gss_stream_type_get_type (void);

void gss_stream_set_type (GssStream *stream, int type);

void gss_stream_add_hls (GssStream *stream);
GssStream * gss_stream_new (int type, int width, int height, int bitrate);
void gss_stream_get_stats (GssStream *stream, guint64 *n_bytes_in,
    guint64 *n_bytes_out);
void gss_stream_resource (GssTransaction * transaction);
const char * gss_stream_type_get_mod (int type);
const char * gss_stream_type_get_ext (int type);
const char * gss_stream_type_get_content_type (int type);

void gss_stream_set_sink (GssStream * stream, GstElement * sink);
void gss_stream_remove_resources (GssStream *stream);
void gss_stream_add_resources (GssStream *stream);

void gss_stream_handle_m3u8 (GssTransaction * t);

void gss_stream_add_fd (GssStream *stream, int fd,
    void (*callback) (GssStream *stream, int fd, void *priv), void *priv);

const char * gss_stream_type_get_name (GssStreamType type);
const char * gss_stream_type_get_id (GssStreamType type);
GssStreamType gss_stream_type_from_id (const char *id);


G_END_DECLS

#endif

