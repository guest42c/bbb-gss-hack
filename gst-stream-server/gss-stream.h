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
#include <gst/rtsp-server/rtsp-server.h>
#include <libsoup/soup.h>
#include "gss-config.h"
#include "gss-types.h"
#include "gss-session.h"
#include "gss-program.h"

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

enum {
  GSS_STREAM_TYPE_UNKNOWN,
  GSS_STREAM_TYPE_OGG,
  GSS_STREAM_TYPE_WEBM,
  GSS_STREAM_TYPE_TS,
  GSS_STREAM_TYPE_TS_MAIN,
  GSS_STREAM_TYPE_FLV
};

struct _GssHLSSegment {
  int index;
  SoupBuffer *buffer;
  char *location;
  int duration;
};

struct _GssStream {
  GObject object;

  GssProgram *program;
  int index;
  char *name;
  char *codecs;
  const char *content_type;
  char *playlist_name;
  const char *ext; /* filaname extension */
  const char *mod; /* stream modifier ('-main') */
  int type;
  char *follow_url;
  int push_fd;
  GssMetrics *metrics;

  GstElement *pipeline;
  GstElement *src;
  GstElement *sink;
  int level;
  int profile;
  int program_id;
  int bandwidth;
  int width;
  int height;
  int bitrate;
  gboolean is_hls;

  GstAdapter *adapter;

  int n_chunks;
  GssHLSSegment chunks[GSS_STREAM_HLS_CHUNKS];
  struct {
    gboolean need_index_update;
    SoupBuffer *index_buffer; /* contents of current index file */

    gboolean at_eos; /* true if sliding window is at the end of the stream */
  } hls;

  GssRtspStream *rtsp_stream;

  void (*custom_client_fd_removed) (GssStream *stream, int fd,
      gpointer user_data);
  gpointer custom_user_data;
};

typedef struct _GssStreamClass GssStreamClass;
struct _GssStreamClass {
  GObjectClass object_class;

};

struct _GssConnection {
  SoupMessage *msg;
  SoupClientContext *client;
  GssStream *stream;
  GssProgram *program;
};


#define GSS_STREAM_MAX_FDS 65536
extern void *gss_stream_fd_table[GSS_STREAM_MAX_FDS];

GType gss_stream_get_type (void);

void gss_stream_set_type (GssStream *stream, int type);

void gss_stream_add_hls (GssStream *stream);
GssStream * gss_stream_new (int type, int width, int height, int bitrate);
void gss_stream_get_stats (GssStream *stream, guint64 *n_bytes_in,
    guint64 *n_bytes_out);
void gss_stream_resource (GssTransaction * transaction);

void gss_stream_set_sink (GssStream * stream, GstElement * sink);
void gss_stream_create_follow_pipeline (GssStream * stream);
void gss_stream_create_push_pipeline (GssStream * stream);
void gss_stream_add_resources (GssStream *stream);

void gss_stream_handle_m3u8 (GssTransaction * t);


G_END_DECLS

#endif

