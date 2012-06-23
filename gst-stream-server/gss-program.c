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
#include "gss-utils.h"

enum
{
  PROP_NONE
};



static void gss_program_get_resource (GssTransaction * transaction);
static void gss_program_put_resource (GssTransaction * transaction);
static void gss_program_frag_resource (GssTransaction * transaction);
static void gss_program_list_resource (GssTransaction * transaction);
static void gss_program_png_resource (GssTransaction * transaction);
static void gss_program_jpeg_resource (GssTransaction * transaction);

static void gss_program_finalize (GObject * object);
static void gss_program_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_program_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GObjectClass *parent_class;


G_DEFINE_TYPE (GssProgram, gss_program, GST_TYPE_OBJECT);

static void
gss_program_init (GssProgram * program)
{

  program->metrics = gss_metrics_new ();

  program->enable_streaming = TRUE;
  program->running = FALSE;
}

static void
gss_program_class_init (GssProgramClass * program_class)
{
  G_OBJECT_CLASS (program_class)->set_property = gss_program_set_property;
  G_OBJECT_CLASS (program_class)->get_property = gss_program_get_property;
  G_OBJECT_CLASS (program_class)->finalize = gss_program_finalize;

  parent_class = g_type_class_peek_parent (program_class);
}

static void
gss_program_finalize (GObject * object)
{
  GssProgram *program = GSS_PROGRAM (object);
  GList *g;

  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;

    gst_object_unparent (GST_OBJECT (stream));
  }
  g_list_free (program->streams);

  if (program->hls.variant_buffer) {
    soup_buffer_free (program->hls.variant_buffer);
  }

  if (program->pngappsink)
    g_object_unref (program->pngappsink);
  if (program->jpegsink)
    g_object_unref (program->jpegsink);
  gss_metrics_free (program->metrics);
  g_free (program->follow_uri);
  g_free (program->follow_host);
}

static void
gss_program_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssProgram *program;

  program = GSS_PROGRAM (object);
  (void) program;

  switch (prop_id) {
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_program_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssProgram *program;

  program = GSS_PROGRAM (object);
  (void) program;

  switch (prop_id) {
    default:
      g_assert_not_reached ();
      break;
  }
}


GssProgram *
gss_program_new (const char *program_name)
{
  return g_object_new (GSS_TYPE_PROGRAM, "name", program_name, NULL);
}

void
gss_program_add_server_resources (GssProgram * program)
{
  char *s;

  s = g_strdup_printf ("/%s", GST_OBJECT_NAME (program));
  gss_server_add_resource (program->server, s, GSS_RESOURCE_UI, "text/html",
      gss_program_get_resource, gss_program_put_resource, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s.frag", GST_OBJECT_NAME (program));
  gss_server_add_resource (program->server, s, GSS_RESOURCE_UI, "text/plain",
      gss_program_frag_resource, NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s.list", GST_OBJECT_NAME (program));
  gss_server_add_resource (program->server, s, GSS_RESOURCE_UI, "text/plain",
      gss_program_list_resource, NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s-snapshot.png", GST_OBJECT_NAME (program));
  gss_server_add_resource (program->server, s, GSS_RESOURCE_UI, "image/png",
      gss_program_png_resource, NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s-snapshot.jpeg", GST_OBJECT_NAME (program));
  gss_server_add_resource (program->server, s, GSS_RESOURCE_HTTP_ONLY,
      "multipart/x-mixed-replace;boundary=myboundary",
      gss_program_jpeg_resource, NULL, NULL, program);
  g_free (s);
}

void
gss_program_remove_server_resources (GssProgram * program)
{
  /* FIXME */
}

void
gss_program_add_stream (GssProgram * program, GssStream * stream)
{
  program->streams = g_list_append (program->streams, stream);

  stream->program = program;
  gss_stream_add_resources (stream);

  gst_object_set_parent (GST_OBJECT (stream), GST_OBJECT (program));
}

void
gss_program_enable_streaming (GssProgram * program)
{
  program->enable_streaming = TRUE;
}

void
gss_program_disable_streaming (GssProgram * program)
{
  GList *g;

  program->enable_streaming = FALSE;
  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;
    g_signal_emit_by_name (stream->sink, "clear");
  }
}

void
gss_program_set_running (GssProgram * program, gboolean running)
{
  program->running = running;
}

void
gss_program_stop (GssProgram * program)
{
  GList *g;

  gss_program_log (program, "stop");

  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;

    gss_stream_set_sink (stream, NULL);
    if (stream->pipeline) {
      gst_element_set_state (stream->pipeline, GST_STATE_NULL);

      g_object_unref (stream->pipeline);
      stream->pipeline = NULL;
    }
  }

  if (program->program_type != GSS_PROGRAM_MANUAL) {
    for (g = program->streams; g; g = g_list_next (g)) {
      GssStream *stream = g->data;
      g_object_unref (stream);
    }
  }
}

void
gss_program_start (GssProgram * program)
{

  gss_program_log (program, "start");

  switch (program->program_type) {
    case GSS_PROGRAM_EW_FOLLOW:
      gss_program_follow_get_list (program);
      break;
    case GSS_PROGRAM_HTTP_FOLLOW:
      gss_program_add_stream_follow (program, GSS_STREAM_TYPE_OGG, 640, 360,
          700000, program->follow_uri);
      break;
    case GSS_PROGRAM_MANUAL:
    case GSS_PROGRAM_ICECAST:
    case GSS_PROGRAM_HTTP_PUT:
      break;
    default:
      g_warning ("not implemented");
      break;
  }
}





/* FIXME use the gss-stream.c function instead of this */
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

/* FIXME use the gss-stream.c function instead of this */
static void
client_fd_removed (GstElement * e, int fd, gpointer user_data)
{
  GssStream *stream = user_data;
  SoupSocket *socket = gss_stream_fd_table[fd];

  if (socket) {
    soup_socket_disconnect (socket);
    gss_stream_fd_table[fd] = NULL;
  } else {
    stream->custom_client_fd_removed (stream, fd, stream->custom_user_data);
  }
}

void
gss_program_set_jpegsink (GssProgram * program, GstElement * jpegsink)
{
  program->jpegsink = g_object_ref (jpegsink);

  g_signal_connect (jpegsink, "client-removed",
      G_CALLBACK (client_removed), NULL);
  g_signal_connect (jpegsink, "client-fd-removed",
      G_CALLBACK (client_fd_removed), NULL);
}

void
gss_program_log (GssProgram * program, const char *message, ...)
{
  char *thetime = gss_utils_get_time_string ();
  char *s;
  va_list varargs;

  g_return_if_fail (program);
  g_return_if_fail (message);

  va_start (varargs, message);
  s = g_strdup_vprintf (message, varargs);
  va_end (varargs);

  gss_server_log (program->server, g_strdup_printf ("%s: %s: %s",
          thetime, GST_OBJECT_NAME (program), s));
  g_free (s);
  g_free (thetime);
}

void
gss_program_add_jpeg_block (GssProgram * program, GString * s)
{
  if (program->running) {
    if (program->jpegsink) {
      gss_html_append_image_printf (s,
          "/%s-snapshot.jpeg", 0, 0, "snapshot image",
          GST_OBJECT_NAME (program));
    } else {
      g_string_append_printf (s, "<img src='/no-snapshot.png'>\n");
    }
  } else {
    g_string_append_printf (s, "<img src='/offline.png'>\n");
  }
}

void
gss_program_add_video_block (GssProgram * program, GString * s, int max_width,
    const char *base_url)
{
  GList *g;
  int width = 0;
  int height = 0;
  int flash_only = TRUE;

  if (!program->running) {
    g_string_append_printf (s, "<img src='/offline.png'>\n");
    return;
  }

  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;
    if (stream->width > width)
      width = stream->width;
    if (stream->height > height)
      height = stream->height;
    if (stream->type != GSS_STREAM_TYPE_FLV) {
      flash_only = FALSE;
    }
  }
  if (max_width != 0 && width > max_width) {
    height = max_width * 9 / 16;
    width = max_width;
  }

  if (enable_video_tag && !flash_only) {
    g_string_append_printf (s,
        "<video controls=\"controls\" autoplay=\"autoplay\" "
        "id=video width=\"%d\" height=\"%d\">\n", width, height);

    for (g = g_list_last (program->streams); g; g = g_list_previous (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_WEBM) {
        g_string_append_printf (s,
            "<source src=\"%s/%s\" type='video/webm; codecs=\"vp8, vorbis\"'>\n",
            base_url, GST_OBJECT_NAME (stream));
      }
    }

    for (g = g_list_last (program->streams); g; g = g_list_previous (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_OGG) {
        g_string_append_printf (s,
            "<source src=\"%s/%s\" type='video/ogg; codecs=\"theora, vorbis\"'>\n",
            base_url, GST_OBJECT_NAME (stream));
      }
    }

    for (g = g_list_last (program->streams); g; g = g_list_previous (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_TS ||
          stream->type == GSS_STREAM_TYPE_TS_MAIN) {
        g_string_append_printf (s,
            "<source src=\"%s/%s.m3u8\" >\n", base_url,
            GST_OBJECT_NAME (program));
        break;
      }
    }

  }

  if (enable_cortado) {
    for (g = program->streams; g; g = g_list_next (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_OGG) {
        g_string_append_printf (s,
            "<applet code=\"com.fluendo.player.Cortado.class\"\n"
            "  archive=\"%s/cortado.jar\" width=\"%d\" height=\"%d\">\n"
            "    <param name=\"url\" value=\"%s/%s\"></param>\n"
            "</applet>\n", base_url, width, height,
            base_url, GST_OBJECT_NAME (stream));
        break;
      }
    }
  }

  if (enable_flash) {
    for (g = program->streams; g; g = g_list_next (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_FLV) {
        g_string_append_printf (s,
            " <object width='%d' height='%d' id='flvPlayer' "
            "type=\"application/x-shockwave-flash\" "
            "data=\"OSplayer.swf\">\n"
            "  <param name='allowFullScreen' value='true'>\n"
            "  <param name=\"allowScriptAccess\" value=\"always\"> \n"
            "  <param name=\"movie\" value=\"OSplayer.swf\"> \n"
            "  <param name=\"flashvars\" value=\""
            "movie=%s/%s"
            "&btncolor=0x333333"
            "&accentcolor=0x31b8e9"
            "&txtcolor=0xdddddd"
            "&volume=30"
            "&autoload=on"
            "&autoplay=off"
            "&vTitle=TITLE"
            "&showTitle=yes\">\n", width, height + 24,
            base_url, GST_OBJECT_NAME (stream));
        if (program->enable_snapshot) {
          gss_html_append_image_printf (s,
              "%s/%s-snapshot.png", 0, 0, "snapshot image",
              base_url, GST_OBJECT_NAME (program));
        }
        g_string_append_printf (s, " </object>\n");
        break;
      }

    }
  } else {
    if (program->enable_snapshot) {
      gss_html_append_image_printf (s,
          "%s/%s-snapshot.png", 0, 0, "snapshot image",
          base_url, GST_OBJECT_NAME (program));
    }
  }

  if (enable_video_tag && !flash_only) {
    g_string_append (s, "</video>\n");
  }

}

static void
gss_program_frag_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GString *s;

  if (!program->enable_streaming) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  t->s = s = g_string_new ("");
  gss_program_add_video_block (program, s, 0, program->server->base_url);
}

static void
gss_program_get_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GString *s = g_string_new ("");
  const char *base_url = "";
  GList *g;
  int i = 0;

  t->s = s;

  gss_html_header (t);

  g_string_append_printf (s, "<h1>%s</h1>\n", GST_OBJECT_NAME (program));

  gss_program_add_video_block (program, s, 0, "");

  gss_html_append_break (s);
  for (g = program->streams; g; g = g_list_next (g), i++) {
    GssStream *stream = g->data;

    gss_html_append_break (s);
    g_string_append_printf (s,
        "%d: %s %dx%d %d kbps <a href=\"%s/%s\">stream</a> "
        "<a href=\"%s/%s\">playlist</a>\n", i,
        gss_stream_type_get_name (stream->type),
        stream->width, stream->height, stream->bitrate / 1000,
        base_url, GST_OBJECT_NAME (stream), base_url, stream->playlist_name);
  }
  if (program->enable_hls) {
    gss_html_append_break (s);
    g_string_append_printf (s,
        "<a href=\"%s/%s.m3u8\">HLS</a>\n", base_url,
        GST_OBJECT_NAME (program));
  }

  gss_html_footer (t);
}


static void
push_wrote_headers (SoupMessage * msg, void *user_data)
{
  GssStream *stream = (GssStream *) user_data;
  SoupSocket *socket;

  socket = soup_client_context_get_socket (stream->program->push_client);
  stream->push_fd = soup_socket_get_fd (socket);

  gss_stream_create_push_pipeline (stream);

  gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
  stream->program->running = TRUE;
}


static void
gss_program_put_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  const char *content_type;
  GssStream *stream;
  gboolean is_icecast;

  /* FIXME should check if another client has connected */
#if 0
  if (program->push_client) {
    gss_program_log (program, "busy");
    soup_message_set_status (t->msg, SOUP_STATUS_CONFLICT);
    return;
  }
#endif

  is_icecast = FALSE;
  if (soup_message_headers_get_one (t->msg->request_headers, "ice-name")) {
    is_icecast = TRUE;
  }

  content_type = soup_message_headers_get_one (t->msg->request_headers,
      "Content-Type");
  if (content_type) {
    if (strcmp (content_type, "application/ogg") == 0) {
      program->push_media_type = GSS_STREAM_TYPE_OGG;
    } else if (strcmp (content_type, "video/webm") == 0) {
      program->push_media_type = GSS_STREAM_TYPE_WEBM;
    } else if (strcmp (content_type, "video/mpeg-ts") == 0) {
      program->push_media_type = GSS_STREAM_TYPE_TS;
    } else if (strcmp (content_type, "video/mp2t") == 0) {
      program->push_media_type = GSS_STREAM_TYPE_TS;
    } else if (strcmp (content_type, "video/x-flv") == 0) {
      program->push_media_type = GSS_STREAM_TYPE_FLV;
    } else {
      program->push_media_type = GSS_STREAM_TYPE_OGG;
    }
  } else {
    program->push_media_type = GSS_STREAM_TYPE_OGG;
  }

  if (program->push_client == NULL) {
    if (is_icecast) {
      program->program_type = GSS_PROGRAM_ICECAST;
    } else {
      program->program_type = GSS_PROGRAM_HTTP_PUT;
    }

    stream = gss_program_add_stream_full (program, program->push_media_type,
        640, 360, 600000, NULL);

    if (!is_icecast) {
      gss_stream_create_push_pipeline (stream);

      gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
      program->running = TRUE;
    }

    gss_program_start (program);

    program->push_client = t->client;
  }

  /* FIXME the user should specify a stream */
  stream = program->streams->data;

  if (is_icecast) {
    soup_message_headers_set_encoding (t->msg->response_headers,
        SOUP_ENCODING_EOF);

    g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (push_wrote_headers),
        stream);
  } else {
    if (t->msg->request_body) {
      GstBuffer *buffer;
      GstFlowReturn flow_ret;

      buffer = gst_buffer_new_and_alloc (t->msg->request_body->length);
      memcpy (GST_BUFFER_DATA (buffer), t->msg->request_body->data,
          t->msg->request_body->length);

      g_signal_emit_by_name (stream->src, "push-buffer", buffer, &flow_ret);
      gst_buffer_unref (buffer);
    }
  }

  soup_message_set_status (t->msg, SOUP_STATUS_OK);
}

static void
gss_program_list_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GString *s = g_string_new ("");
  GList *g;
  const char *base_url = "";
  int i = 0;

  t->s = s;

  for (g = program->streams; g; g = g_list_next (g), i++) {
    GssStream *stream = g->data;
    const char *typename = "unknown";
    switch (stream->type) {
      case GSS_STREAM_TYPE_OGG:
        typename = "ogg";
        break;
      case GSS_STREAM_TYPE_WEBM:
        typename = "webm";
        break;
      case GSS_STREAM_TYPE_TS:
        typename = "mpeg-ts";
        break;
      case GSS_STREAM_TYPE_TS_MAIN:
        typename = "mpeg-ts-main";
        break;
      case GSS_STREAM_TYPE_FLV:
        typename = "flv";
        break;
      default:
        g_assert_not_reached ();
        break;
    }
    g_string_append_printf (s,
        "%d %s %d %d %d %s/%s\n", i, typename,
        stream->width, stream->height, stream->bitrate, base_url,
        GST_OBJECT_NAME (stream));
  }
}

static void
gss_program_png_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GstBuffer *buffer = NULL;

  if (!program->enable_streaming || !program->running) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (program->pngappsink) {
    g_object_get (program->pngappsink, "last-buffer", &buffer, NULL);
  }

  if (buffer) {
    soup_message_set_status (t->msg, SOUP_STATUS_OK);

    soup_message_set_response (t->msg, "image/png", SOUP_MEMORY_COPY,
        (void *) GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

    gst_buffer_unref (buffer);
  } else {
    gss_html_error_404 (t->msg);
  }

}

static void
jpeg_wrote_headers (SoupMessage * msg, void *user_data)
{
  GssConnection *connection = user_data;
  SoupSocket *socket;
  int fd;

  socket = soup_client_context_get_socket (connection->client);
  fd = soup_socket_get_fd (socket);

  if (connection->program->jpegsink) {
    g_signal_emit_by_name (connection->program->jpegsink, "add", fd);

    g_assert (fd < GSS_STREAM_MAX_FDS);
    gss_stream_fd_table[fd] = socket;
  } else {
    soup_socket_disconnect (socket);
  }

  g_free (connection);
}

static void
gss_program_jpeg_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GssConnection *connection;

  if (!program->enable_streaming || program->jpegsink == NULL) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  connection = g_malloc0 (sizeof (GssConnection));
  connection->msg = t->msg;
  connection->client = t->client;
  connection->program = program;

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (t->msg->response_headers,
      SOUP_ENCODING_EOF);
  soup_message_headers_replace (t->msg->response_headers, "Content-Type",
      "multipart/x-mixed-replace;boundary=myboundary");

  g_signal_connect (t->msg, "wrote-headers", G_CALLBACK (jpeg_wrote_headers),
      connection);
}
