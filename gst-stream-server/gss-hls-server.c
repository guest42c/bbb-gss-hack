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


#include "config.h"

#include "gss-server.h"



enum
{
  PROP_NAME = 1
};


static void gss_hls_handle_m3u8 (GssTransaction * t);
static void gss_hls_handle_stream_m3u8 (GssTransaction * t);
static void gss_hls_handle_ts_chunk (GssTransaction * t);

void gss_program_add_hls_chunk (GssStream * stream, SoupBuffer * buf);

static gboolean
sink_data_probe_callback (GstPad * pad, GstMiniObject * mo, gpointer user_data);

static void gss_hls_update_variant (GssProgram * program);

void
gss_stream_add_hls (GssStream * stream)
{
  GssProgram *program = stream->program;
  char *s;
  int mbs_per_sec;
  int level;
  int profile;

  if (!program->enable_hls) {

    program->hls.target_duration = 4;

    program->enable_hls = TRUE;

    s = g_strdup_printf ("/%s.m3u8", GST_OBJECT_NAME (program));
    gss_server_add_resource (program->server, s, 0,
        "video/x-mpegurl", gss_hls_handle_m3u8, NULL, NULL, program);
    g_free (s);
  }

  gst_pad_add_data_probe (gst_element_get_pad (stream->sink, "sink"),
      G_CALLBACK (sink_data_probe_callback), stream);

  profile = 0;
  if (stream->type == GSS_STREAM_TYPE_M2TS_H264BASE_AAC) {
    profile = 0x42e0;           /* baseline */
  } else if (stream->type == GSS_STREAM_TYPE_M2TS_H264MAIN_AAC) {
    profile = 0x4d40;           /* main */
  }

  mbs_per_sec = ((stream->width + 15) >> 4) * ((stream->height + 15) >> 4) * 30;
  if (mbs_per_sec <= 1485) {
    level = 10;
  } else if (mbs_per_sec <= 3000) {
    level = 11;
  } else if (mbs_per_sec <= 6000) {
    level = 12;
  } else if (mbs_per_sec <= 19800) {
    level = 21;
  } else if (mbs_per_sec <= 20250) {
    level = 22;
  } else if (mbs_per_sec <= 40500) {
    level = 30;
  } else if (mbs_per_sec <= 108000) {
    level = 31;
  } else if (mbs_per_sec <= 216000) {
    level = 32;
  } else if (mbs_per_sec <= 245760) {
    level = 40;
  } else if (mbs_per_sec <= 522240) {
    level = 42;
  } else if (mbs_per_sec <= 589824) {
    level = 50;
  } else if (mbs_per_sec <= 983040) {
    level = 51;
  } else {
    level = 0xff;
  }

  stream->codecs = g_strdup_printf ("avc1.%04X%02X, mp4a.40.2", profile, level);
  stream->is_hls = TRUE;

  stream->adapter = gst_adapter_new ();

  s = g_strdup_printf ("/%s-%dx%d-%dkbps%s.m3u8", GST_OBJECT_NAME (program),
      stream->width, stream->height, stream->bitrate / 1000, stream->mod);
  gss_server_add_resource (program->server, s, 0,
      "video/x-mpegurl", gss_hls_handle_stream_m3u8, NULL, NULL, stream);
  g_free (s);

  gss_hls_update_variant (program);
}


typedef struct _ChunkCallback ChunkCallback;
struct _ChunkCallback
{
  GssStream *stream;
  guint8 *data;
  int n;
};

static gboolean
gss_program_add_hls_chunk_callback (gpointer data)
{
  ChunkCallback *chunk_callback = (ChunkCallback *) data;
  SoupBuffer *buffer;

  buffer =
      soup_buffer_new (SOUP_MEMORY_TAKE, chunk_callback->data,
      chunk_callback->n);
  gss_program_add_hls_chunk (chunk_callback->stream, buffer);

  g_free (chunk_callback);

  return FALSE;
}

static gboolean
sink_data_probe_callback (GstPad * pad, GstMiniObject * mo, gpointer user_data)
{
  GssStream *stream = (GssStream *) user_data;

  if (GST_IS_BUFFER (mo)) {
    GstBuffer *buffer = GST_BUFFER (mo);
    guint8 *data = GST_BUFFER_DATA (buffer);

    if (((data[3] >> 4) & 2) && ((data[5] >> 6) & 1)) {
      int n;

      n = gst_adapter_available (stream->adapter);
      if (n < 188 * 100) {
        /* skipped (too early) */
      } else {
        ChunkCallback *chunk_callback;

        chunk_callback = g_malloc0 (sizeof (ChunkCallback));
        chunk_callback->data = gst_adapter_take (stream->adapter, n);
        chunk_callback->n = n;
        chunk_callback->stream = stream;

        g_idle_add (gss_program_add_hls_chunk_callback, chunk_callback);
      }
    }

    gst_adapter_push (stream->adapter, gst_buffer_ref (buffer));
  } else {
    /* got event */
  }

  return TRUE;
}

void
gss_program_add_hls_chunk (GssStream * stream, SoupBuffer * buf)
{
  GssHLSSegment *segment;

  segment = &stream->chunks[stream->n_chunks % GSS_STREAM_HLS_CHUNKS];

  if (segment->buffer) {
    gss_server_remove_resource (stream->program->server, segment->location);
    g_free (segment->location);
    soup_buffer_free (segment->buffer);
  }
  segment->index = stream->n_chunks;
  segment->buffer = buf;
  segment->location = g_strdup_printf ("/%s-%dx%d-%dkbps%s-%05d.ts",
      GST_OBJECT_NAME (stream->program), stream->width, stream->height,
      stream->bitrate / 1000, stream->mod, stream->n_chunks);
  segment->duration = stream->program->hls.target_duration;

  stream->hls.need_index_update = TRUE;

  gss_server_add_resource (stream->program->server, segment->location,
      0, "video/mp2t", gss_hls_handle_ts_chunk, NULL, NULL, segment);

  stream->n_chunks++;
  stream->program->n_hls_chunks = stream->n_chunks;

  if (stream->n_chunks == 1) {
    gss_hls_update_variant (stream->program);
  }

}


static void
gss_hls_update_index (GssStream * stream)
{
  GssProgram *program = stream->program;
  GString *s;
  int i;
  int seq_num = MAX (0, program->n_hls_chunks - 5);

  s = g_string_new ("#EXTM3U\n");

  g_string_append_printf (s, "#EXT-X-TARGETDURATION:%d\n",
      program->hls.target_duration);
  g_string_append_printf (s, "#EXT-X-MEDIA-SEQUENCE:%d\n", seq_num);
  if (program->hls.is_encrypted) {
    g_string_append_printf (s, "#EXT-X-KEY:METHOD=AES-128,URI=\"%s\"",
        program->hls.key_uri);
    if (program->hls.have_iv) {
      g_string_append_printf (s, ",IV=0x%08x%08x%08x%08x",
          program->hls.init_vector[0], program->hls.init_vector[1],
          program->hls.init_vector[2], program->hls.init_vector[3]);
    } else {
      g_string_append (s, "\n");
    }
  } else {
    g_string_append (s, "#EXT-X-KEY:METHOD=NONE\n");
  }

  if (0) {
    g_string_append (s, "#EXT-X-PROGRAM-DATE-TIME:YYYY-MM-DDThh:mm:ssZ\n");
  }
  g_string_append (s, "#EXT-X-ALLOW-CACHE:NO\n");
  g_string_append (s, "#EXT-X-VERSION:1\n");

  for (i = seq_num; i < stream->n_chunks; i++) {
    GssHLSSegment *segment = &stream->chunks[i % GSS_STREAM_HLS_CHUNKS];

    g_string_append_printf (s,
        "#EXTINF:%d,\n"
        "%s%s\n",
        segment->duration, program->server->base_url, segment->location);
  }

  if (stream->hls.at_eos) {
    g_string_append (s, "#EXT-X-ENDLIST\n");
  }

  if (stream->hls.index_buffer) {
    soup_buffer_free (stream->hls.index_buffer);
  }
  stream->hls.index_buffer = soup_buffer_new (SOUP_MEMORY_TAKE, s->str, s->len);
  g_string_free (s, FALSE);

  stream->hls.need_index_update = FALSE;
}

static void
gss_hls_update_variant (GssProgram * program)
{
  GList *g;
  GString *s;

  s = g_string_new ("#EXTM3U\n");
  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;

    if (!stream->is_hls)
      continue;
    if (stream->bitrate == 0)
      continue;
    if (stream->n_chunks == 0)
      continue;

    g_string_append_printf (s,
        "#EXT-X-STREAM-INF:PROGRAM-ID=%d,BANDWIDTH=%d,"
        "CODECS=\"%s\",RESOLUTION=\"%dx%d\"\n",
        stream->program_id,
        stream->bitrate, stream->codecs, stream->width, stream->height);
    g_string_append_printf (s, "%s/%s-%dx%d-%dkbps%s.m3u8\n",
        program->server->base_url,
        GST_OBJECT_NAME (program),
        stream->width, stream->height, stream->bitrate / 1000, stream->mod);
  }
  if (program->hls.variant_buffer) {
    soup_buffer_free (program->hls.variant_buffer);
  }
  program->hls.variant_buffer =
      soup_buffer_new (SOUP_MEMORY_TAKE, s->str, s->len);
  g_string_free (s, FALSE);

}

static void
gss_hls_handle_m3u8 (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;

  g_assert (program->hls.variant_buffer != NULL);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);
  soup_message_headers_replace (t->msg->response_headers,
      "Cache-Control", "no-store");
  soup_message_body_append_buffer (t->msg->response_body,
      program->hls.variant_buffer);
}

static void
gss_hls_handle_stream_m3u8 (GssTransaction * t)
{
  GssStream *stream = (GssStream *) t->resource->priv;

  if (stream->hls.index_buffer == NULL || stream->hls.need_index_update) {
    gss_hls_update_index (stream);
  }

  soup_message_set_status (t->msg, SOUP_STATUS_OK);
  soup_message_headers_replace (t->msg->response_headers,
      "Cache-Control", "no-store");
  soup_message_body_append_buffer (t->msg->response_body,
      stream->hls.index_buffer);
}

static void
gss_hls_handle_ts_chunk (GssTransaction * t)
{
  GssHLSSegment *segment = (GssHLSSegment *) t->resource->priv;

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_headers_replace (t->msg->response_headers,
      "Cache-Control", "no-store");

  soup_message_body_append_buffer (t->msg->response_body, segment->buffer);
}

void
gss_stream_handle_m3u8 (GssTransaction * t)
{
  GssStream *stream = (GssStream *) t->resource->priv;
  char *content;

  content = g_strdup_printf ("#EXTM3U\n"
      "#EXT-X-TARGETDURATION:10\n"
      "#EXTINF:10,\n"
      "%s/%s\n", stream->program->server->base_url, GST_OBJECT_NAME (stream));

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_body_append (t->msg->response_body, SOUP_MEMORY_TAKE,
      content, strlen (content));
}
