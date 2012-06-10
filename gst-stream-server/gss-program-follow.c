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

static void handle_pipeline_message (GstBus * bus, GstMessage * message,
    gpointer user_data);

/* set up a stream follower */

void
gss_program_add_stream_follow (GssProgram * program, int type, int width,
    int height, int bitrate, const char *url)
{
  GssStream *stream;

  stream = gss_program_add_stream_full (program, type, width, height,
      bitrate, NULL);
  stream->follow_url = g_strdup (url);

  gss_stream_create_follow_pipeline (stream);

  gst_element_set_state (stream->pipeline, GST_STATE_PLAYING);
}

void
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
    case GSS_SERVER_STREAM_OGG:
      g_string_append (pipe_desc, "oggparse name=parse ! ");
      break;
    case GSS_SERVER_STREAM_TS:
    case GSS_SERVER_STREAM_TS_MAIN:
      g_string_append (pipe_desc, "mpegtsparse name=parse ! ");
      break;
    case GSS_SERVER_STREAM_WEBM:
      g_string_append (pipe_desc, "matroskaparse name=parse ! ");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  g_string_append (pipe_desc, "queue ! ");
  g_string_append_printf (pipe_desc, "%s name=sink ",
      gss_server_get_multifdsink_string ());

  if (verbose) {
    g_print ("pipeline: %s\n", pipe_desc->str);
  }
  error = NULL;
  pipe = gst_parse_launch (pipe_desc->str, &error);
  if (error != NULL) {
    if (verbose)
      g_print ("pipeline parse error: %s\n", error->message);
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

      if (0 && verbose)
        g_print ("message: %s (%s,%s,%s) from %s\n",
            GST_MESSAGE_TYPE_NAME (message),
            gst_element_state_get_name (newstate),
            gst_element_state_get_name (oldstate),
            gst_element_state_get_name (pending),
            GST_MESSAGE_SRC_NAME (message));

      if (newstate == GST_STATE_PLAYING
          && message->src == GST_OBJECT (stream->pipeline)) {
        char *s;
        s = g_strdup_printf ("stream %s started", stream->name);
        gss_program_log (program, s);
        g_free (s);
        program->running = TRUE;
      }
    }
      break;
    case GST_MESSAGE_STREAM_STATUS:
    {
      GstStreamStatusType type;
      GstElement *owner;

      gst_message_parse_stream_status (message, &type, &owner);

      if (0 && verbose)
        g_print ("message: %s (%d) from %s\n", GST_MESSAGE_TYPE_NAME (message),
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
      gss_program_log (program, s);
      g_free (s);

      program->restart_delay = 5;
      gss_program_stop (program);
    }
      break;
    case GST_MESSAGE_EOS:
      gss_program_log (program, "end of stream");
      gss_program_stop (program);
      switch (program->program_type) {
        case GSS_PROGRAM_EW_FOLLOW:
        case GSS_PROGRAM_HTTP_FOLLOW:
          program->restart_delay = 5;
          break;
        case GSS_PROGRAM_HTTP_PUT:
        case GSS_PROGRAM_ICECAST:
          program->push_client = NULL;
          break;
        case GSS_PROGRAM_EW_CONTRIB:
          break;
        case GSS_PROGRAM_MANUAL:
        default:
          break;
      }
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
  GssProgram *program = ptr;

  if (message->status_code == SOUP_STATUS_OK) {
    SoupBuffer *buffer;
    char **lines;
    int i;

    gss_program_log (program, "got list of streams");

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

        type = GSS_SERVER_STREAM_UNKNOWN;
        if (strcmp (type_str, "ogg") == 0) {
          type = GSS_SERVER_STREAM_OGG;
        } else if (strcmp (type_str, "webm") == 0) {
          type = GSS_SERVER_STREAM_WEBM;
        } else if (strcmp (type_str, "mpeg-ts") == 0) {
          type = GSS_SERVER_STREAM_TS;
        } else if (strcmp (type_str, "mpeg-ts-main") == 0) {
          type = GSS_SERVER_STREAM_TS_MAIN;
        } else if (strcmp (type_str, "flv") == 0) {
          type = GSS_SERVER_STREAM_FLV;
        }

        full_url = g_strdup_printf ("http://%s%s", program->follow_host, url);
        gss_program_add_stream_follow (program, type, width, height, bitrate,
            full_url);
        g_free (full_url);
      }

    }

    g_strfreev (lines);

    soup_buffer_free (buffer);
  } else {
    gss_program_log (program, "failed to get list of streams");
    program->restart_delay = 10;
    gss_program_stop (program);
  }

}

void
gss_program_follow (GssProgram * program, const char *host, const char *stream)
{
  program->program_type = GSS_PROGRAM_EW_FOLLOW;
  program->follow_uri = g_strdup_printf ("http://%s/%s.list", host, stream);
  program->follow_host = g_strdup (host);
  program->restart_delay = 1;
}

void
gss_program_follow_get_list (GssProgram * program)
{
  SoupMessage *message;

  message = soup_message_new ("GET", program->follow_uri);

  soup_session_queue_message (program->server->client_session, message,
      follow_callback, program);
}

void
gss_program_http_follow (GssProgram * program, const char *uri)
{
  program->program_type = GSS_PROGRAM_HTTP_FOLLOW;
  program->follow_uri = g_strdup (uri);
  program->follow_host = g_strdup (uri);

  program->restart_delay = 1;
}
