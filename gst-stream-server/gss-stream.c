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
#include "gss-html.h"
#include "gss-session.h"
#include "gss-soup.h"
#include "gss-rtsp.h"
#include "gss-content.h"


#define verbose FALSE

static void msg_wrote_headers (SoupMessage * msg, void *user_data);

void *gss_stream_fd_table[GSS_STREAM_MAX_FDS];

void
gss_stream_free (GssStream * stream)
{
  int i;

  g_free (stream->name);
  g_free (stream->playlist_name);
  g_free (stream->codecs);
  g_free (stream->content_type);
  g_free (stream->follow_url);

  for (i = 0; i < GSS_STREAM_HLS_CHUNKS; i++) {
    GssHLSSegment *segment = &stream->chunks[i];

    if (segment->buffer) {
      soup_buffer_free (segment->buffer);
      g_free (segment->location);
    }
  }

  if (stream->hls.index_buffer) {
    soup_buffer_free (stream->hls.index_buffer);
  }
#define CLEANUP(x) do { \
  if (x) { \
    if (verbose && GST_OBJECT_REFCOUNT (x) != 1) \
      g_print( #x "refcount %d\n", GST_OBJECT_REFCOUNT (x)); \
    g_object_unref (x); \
  } \
} while (0)

  gss_stream_set_sink (stream, NULL);
  CLEANUP (stream->src);
  CLEANUP (stream->sink);
  CLEANUP (stream->adapter);
  CLEANUP (stream->rtsp_stream);
  if (stream->pipeline) {
    gst_element_set_state (GST_ELEMENT (stream->pipeline), GST_STATE_NULL);
    CLEANUP (stream->pipeline);
  }
  if (stream->adapter)
    g_object_unref (stream->adapter);
  gss_metrics_free (stream->metrics);

  g_free (stream);
}

void
gss_stream_get_stats (GssStream * stream, guint64 * in, guint64 * out)
{
  g_object_get (stream->sink, "bytes-to-serve", in, "bytes-served", out, NULL);
}

static void
client_removed (GstElement * e, int fd, int status, gpointer user_data)
{
  GssStream *stream = user_data;

  if (gss_stream_fd_table[fd]) {
    if (stream) {
      gss_metrics_remove_client (stream->metrics, stream->bitrate);
      gss_metrics_remove_client (stream->program->metrics, stream->bitrate);
      gss_metrics_remove_client (stream->program->server->metrics,
          stream->bitrate);
    }
  }
}

static void
client_fd_removed (GstElement * e, int fd, gpointer user_data)
{
  GssStream *stream = user_data;
  SoupSocket *sock = gss_stream_fd_table[fd];

  if (sock) {
    soup_socket_disconnect (sock);
    gss_stream_fd_table[fd] = NULL;
  } else {
    stream->custom_client_fd_removed (stream, fd, stream->custom_user_data);
  }
}

static void
stream_resource (GssTransaction * t)
{
  GssStream *stream = (GssStream *) t->resource->priv;
  GssConnection *connection;

  if (!stream->program->enable_streaming || !stream->program->running) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (t->server->metrics->n_clients >= t->server->max_connections ||
      t->server->metrics->bitrate + stream->bitrate >= t->server->max_bitrate) {
    if (verbose)
      g_print ("n_clients %d max_connections %d\n",
          t->server->metrics->n_clients, t->server->max_connections);
    if (verbose)
      g_print ("current bitrate %" G_GINT64_FORMAT " bitrate %d max_bitrate %"
          G_GINT64_FORMAT "\n", t->server->metrics->bitrate, stream->bitrate,
          t->server->max_bitrate);
    soup_message_set_status (t->msg, SOUP_STATUS_SERVICE_UNAVAILABLE);
    return;
  }

  connection = g_malloc0 (sizeof (GssConnection));
  connection->msg = t->msg;
  connection->client = t->client;
  connection->stream = stream;

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (t->msg->response_headers,
      SOUP_ENCODING_EOF);
  soup_message_headers_replace (t->msg->response_headers, "Content-Type",
      stream->content_type);

  g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (msg_wrote_headers),
      connection);
}

static void
msg_wrote_headers (SoupMessage * msg, void *user_data)
{
  GssConnection *connection = user_data;
  SoupSocket *sock;
  int fd;

  sock = soup_client_context_get_socket (connection->client);
  fd = soup_socket_get_fd (sock);

  if (connection->stream->sink) {
    GssStream *stream = connection->stream;

    g_signal_emit_by_name (connection->stream->sink, "add", fd);

    g_assert (fd < GSS_STREAM_MAX_FDS);
    gss_stream_fd_table[fd] = sock;

    gss_metrics_add_client (stream->metrics, stream->bitrate);
    gss_metrics_add_client (stream->program->metrics, stream->bitrate);
    gss_metrics_add_client (stream->program->server->metrics, stream->bitrate);
  } else {
    soup_socket_disconnect (sock);
  }

  g_free (connection);
}

GssStream *
gss_stream_new (int type, int width, int height, int bitrate)
{
  GssStream *stream;

  stream = g_malloc0 (sizeof (GssStream));

  stream->metrics = gss_metrics_new ();

  stream->type = type;
  stream->width = width;
  stream->height = height;
  stream->bitrate = bitrate;

  switch (type) {
    case GSS_SERVER_STREAM_OGG:
      stream->content_type = g_strdup ("video/ogg");
      stream->mod = "";
      stream->ext = "ogv";
      break;
    case GSS_SERVER_STREAM_WEBM:
      stream->content_type = g_strdup ("video/webm");
      stream->mod = "";
      stream->ext = "webm";
      break;
    case GSS_SERVER_STREAM_TS:
      stream->content_type = g_strdup ("video/mp2t");
      stream->mod = "";
      stream->ext = "ts";
      break;
    case GSS_SERVER_STREAM_TS_MAIN:
      stream->content_type = g_strdup ("video/mp2t");
      stream->mod = "-main";
      stream->ext = "ts";
      break;
    case GSS_SERVER_STREAM_FLV:
      stream->content_type = g_strdup ("video/x-flv");
      stream->mod = "";
      stream->ext = "flv";
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return stream;
}

GssStream *
gss_program_add_stream_full (GssProgram * program,
    int type, int width, int height, int bitrate, GstElement * sink)
{
  GssStream *stream;

  stream = gss_stream_new (type, width, height, bitrate);

  gss_program_add_stream (program, stream);

  /* FIXME this should be called before adding the stream, but it fails */
  gss_stream_set_sink (stream, sink);

  return stream;
}

void
gss_stream_add_resources (GssStream * stream)
{
  char *s;

  if (enable_rtsp) {
    if (stream->type == GSS_SERVER_STREAM_OGG) {
      stream->rtsp_stream = gss_rtsp_stream_new (stream);
      gss_rtsp_stream_start (stream->rtsp_stream);
    }
  }

  stream->name =
      g_strdup_printf ("%s-%dx%d-%dkbps%s.%s", stream->program->location,
      stream->width, stream->height, stream->bitrate / 1000, stream->mod,
      stream->ext);
  s = g_strdup_printf ("/%s", stream->name);
  gss_server_add_resource (stream->program->server, s, GSS_RESOURCE_HTTP_ONLY,
      stream->content_type, stream_resource, NULL, NULL, stream);
  g_free (s);

  stream->playlist_name = g_strdup_printf ("%s-%dx%d-%dkbps%s-%s.m3u8",
      stream->program->location,
      stream->width, stream->height, stream->bitrate / 1000, stream->mod,
      stream->ext);
  s = g_strdup_printf ("/%s", stream->playlist_name);
  gss_server_add_resource (stream->program->server, s, 0,
      "application/x-mpegurl", gss_stream_handle_m3u8, NULL, NULL, stream);
  g_free (s);

  return;
}

void
gss_stream_set_sink (GssStream * stream, GstElement * sink)
{
  if (stream->sink) {
    g_object_unref (stream->sink);
  }

  stream->sink = sink;
  if (stream->sink) {
    g_object_ref (stream->sink);
    g_signal_connect (stream->sink, "client-removed",
        G_CALLBACK (client_removed), stream);
    g_signal_connect (stream->sink, "client-fd-removed",
        G_CALLBACK (client_fd_removed), stream);
    if (stream->type == GSS_SERVER_STREAM_TS ||
        stream->type == GSS_SERVER_STREAM_TS_MAIN) {
      gss_stream_add_hls (stream);
    }
  }
}
