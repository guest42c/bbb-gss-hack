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

#include "gss-pull.h"
#include "gss-server.h"
#include "gss-html.h"
#include "gss-session.h"
#include "gss-soup.h"
#include "gss-content.h"
#include "gss-utils.h"

enum
{
  PROP_NONE,
  PROP_PULL_URI
};

#define DEFAULT_PULL_URI "http://example.com/stream.webm"


static void gss_pull_finalize (GObject * object);
static void gss_pull_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_pull_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void handle_pipeline_message (GstBus * bus, GstMessage * message,
    gpointer user_data);

static void gss_pull_get_list (GssPull * pull);
static void gss_pull_add_stream_follow (GssPull * program, int type,
    int width, int height, int bitrate, const char *url);
static void gss_stream_create_follow_pipeline (GssStream * stream);

static GObjectClass *parent_class;

G_DEFINE_TYPE (GssPull, gss_pull, GSS_TYPE_PROGRAM);

static void
gss_pull_init (GssPull * program)
{
  program->pull_uri = g_strdup (DEFAULT_PULL_URI);
}

static void
gss_pull_class_init (GssPullClass * program_class)
{
  G_OBJECT_CLASS (program_class)->set_property = gss_pull_set_property;
  G_OBJECT_CLASS (program_class)->get_property = gss_pull_get_property;
  G_OBJECT_CLASS (program_class)->finalize = gss_pull_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (program_class),
      PROP_PULL_URI, g_param_spec_string ("pull-uri", "Pull URI",
          "URI for the stream or program to pull from.", DEFAULT_PULL_URI,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  parent_class = g_type_class_peek_parent (program_class);
}

static void
gss_pull_finalize (GObject * object)
{
  GssPull *pull = GSS_PULL (object);

  g_free (pull->pull_uri);

  parent_class->finalize (object);
}

static void
gss_pull_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssPull *pull;

  pull = GSS_PULL (object);

  switch (prop_id) {
    case PROP_PULL_URI:
      g_free (pull->pull_uri);
      pull->pull_uri = g_value_dup_string (value);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gss_pull_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GssPull *pull;

  pull = GSS_PULL (object);

  switch (prop_id) {
    case PROP_PULL_URI:
      g_value_set_string (value, pull->pull_uri);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}


GssProgram *
gss_pull_new (void)
{
  return g_object_new (GSS_TYPE_PULL, NULL);
}


void
gss_pull_stop (GssProgram * program)
{
  GList *g;

  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;

    gss_stream_set_sink (stream, NULL);
    if (stream->pipeline) {
      gst_element_set_state (stream->pipeline, GST_STATE_NULL);

      g_object_unref (stream->pipeline);
      stream->pipeline = NULL;
    }
  }

  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;
    g_object_unref (stream);
  }
}

void
gss_pull_start (GssProgram * program)
{
  GssPull *pull = GSS_PULL (program);

  if (pull->is_ew) {
    gss_pull_get_list (pull);
  } else {
    gss_pull_add_stream_follow (pull,
        GSS_STREAM_TYPE_OGG_THEORA_VORBIS, 640, 360, 700000,
        program->follow_uri);
  }
}

static void
gss_pull_add_stream_follow (GssPull * pull, int type, int width,
    int height, int bitrate, const char *url)
{
  GssStream *stream;

  stream = gss_program_add_stream_full (GSS_PROGRAM (pull),
      type, width, height, bitrate, NULL);
  stream->follow_url = g_strdup (url);

  gss_stream_create_follow_pipeline (stream);

  gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
}

static void
gss_stream_create_follow_pipeline (GssStream * stream)
{
  GstElement *pipe;
  GstElement *e;
  GString *pipe_desc;
  GError *error = NULL;
  GstBus *bus;

  pipe_desc = g_string_new ("");

  g_string_append_printf (pipe_desc,
      "souphttpsrc name=src do-timestamp=true ! ");
  switch (stream->type) {
    case GSS_STREAM_TYPE_OGG_THEORA_VORBIS:
    case GSS_STREAM_TYPE_OGG_THEORA_OPUS:
      g_string_append (pipe_desc, "oggparse name=parse ! ");
      break;
    case GSS_STREAM_TYPE_M2TS_H264BASE_AAC:
    case GSS_STREAM_TYPE_M2TS_H264MAIN_AAC:
      g_string_append (pipe_desc, "mpegtsparse name=parse ! ");
      break;
    case GSS_STREAM_TYPE_WEBM:
      g_string_append (pipe_desc, "matroskaparse name=parse ! ");
      break;
    case GSS_STREAM_TYPE_FLV_H264BASE_AAC:
      g_string_append (pipe_desc, "flvparse name=parse ! ");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  g_string_append (pipe_desc, "queue ! ");
  g_string_append_printf (pipe_desc, "%s name=sink ",
      gss_server_get_multifdsink_string ());

  GST_DEBUG ("pipeline: %s", pipe_desc->str);
  error = NULL;
  pipe = gst_parse_launch (pipe_desc->str, &error);
  if (error != NULL) {
    GST_WARNING ("pipeline parse error: %s", error->message);
    g_error_free (error);
    return;
  }
  g_string_free (pipe_desc, TRUE);

  e = gst_bin_get_by_name (GST_BIN (pipe), "src");
  g_assert (e != NULL);
  g_object_set (e, "location", stream->follow_url, NULL);
  g_object_unref (e);

  e = gst_bin_get_by_name (GST_BIN (pipe), "sink");
  g_assert (e != NULL);
  gss_stream_set_sink (stream, e);
  g_object_unref (e);
  stream->pipeline = pipe;

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipe));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (handle_pipeline_message),
      stream);
  g_object_unref (bus);

}

static void
handle_pipeline_message (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GssStream *stream = user_data;
  GssProgram *program = stream->program;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState newstate;
      GstState oldstate;
      GstState pending;

      gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);

      GST_DEBUG ("message: %s (%s,%s,%s) from %s",
          GST_MESSAGE_TYPE_NAME (message),
          gst_element_state_get_name (newstate),
          gst_element_state_get_name (oldstate),
          gst_element_state_get_name (pending), GST_MESSAGE_SRC_NAME (message));

      if (newstate == GST_STATE_PLAYING
          && message->src == GST_OBJECT (stream->pipeline)) {
        char *s;
        s = g_strdup_printf ("stream %s started", GST_OBJECT_NAME (stream));
        GST_DEBUG_OBJECT (program, s);
        g_free (s);
        gss_program_set_state (program, GSS_PROGRAM_STATE_RUNNING);
      }
    }
      break;
    case GST_MESSAGE_STREAM_STATUS:
    {
      GstStreamStatusType type;
      GstElement *owner;

      gst_message_parse_stream_status (message, &type, &owner);

      GST_DEBUG ("message: %s (%d) from %s", GST_MESSAGE_TYPE_NAME (message),
          type, GST_MESSAGE_SRC_NAME (message));
    }
      break;
    case GST_MESSAGE_ERROR:
    {
      GError *error;
      gchar *debug;
      char *s;

      gst_message_parse_error (message, &error, &debug);

      s = g_strdup_printf ("Internal Error: %s (%s) from %s\n",
          error->message, debug, GST_MESSAGE_SRC_NAME (message));
      GST_DEBUG_OBJECT (program, s);
      g_free (s);

      program->restart_delay = 5;
      gss_program_stop (program);
    }
      break;
    case GST_MESSAGE_EOS:
      GST_DEBUG_OBJECT (program, "end of stream");
      gss_program_stop (program);
      program->restart_delay = 5;
      break;
    case GST_MESSAGE_ELEMENT:
      break;
    default:
      break;
  }
}

static void
follow_callback (SoupSession * session, SoupMessage * message, gpointer ptr)
{
  GssPull *pull = ptr;

  if (message->status_code == SOUP_STATUS_OK) {
    SoupBuffer *buffer;
    char **lines;
    int i;

    GST_DEBUG_OBJECT (pull, "got list of streams");

    buffer = soup_message_body_flatten (message->response_body);

    lines = g_strsplit (buffer->data, "\n", -1);

    for (i = 0; lines[i]; i++) {
      int n;
      int index;
      char type_str[10];
      char url[200];
      int width;
      int height;
      int bitrate;
      int type;

      n = sscanf (lines[i], "%d %9s %d %d %d %199s\n",
          &index, type_str, &width, &height, &bitrate, url);

      if (n == 6) {
        char *full_url;

        type = gss_stream_type_from_id (type_str);

        full_url = g_strdup_printf ("%s%s", pull->pull_uri, url);
        gss_pull_add_stream_follow (pull, type, width, height, bitrate,
            full_url);
        g_free (full_url);
      }

    }

    g_strfreev (lines);

    soup_buffer_free (buffer);
  } else {
    GST_DEBUG_OBJECT (pull, "failed to get list of streams");
    pull->program.restart_delay = 10;
    gss_program_stop (GSS_PROGRAM (pull));
  }

}

void
gss_pull_follow (GssPull * pull, const char *host, const char *stream)
{
  pull->is_ew = TRUE;
  pull->pull_uri = g_strdup_printf ("http://%s/%s.list", host, stream);
}

static void
gss_pull_get_list (GssPull * pull)
{
  SoupMessage *message;

  message = soup_message_new ("GET", pull->pull_uri);

  soup_session_queue_message (pull->program.server->client_session, message,
      follow_callback, pull);
}

void
gss_pull_http_follow (GssPull * pull, const char *uri)
{
  pull->is_ew = FALSE;
  pull->pull_uri = g_strdup (uri);
}
