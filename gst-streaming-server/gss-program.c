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
#include "gss-content.h"
#include "gss-utils.h"

enum
{
  PROP_NONE,
  PROP_ENABLED,
  PROP_STATE,
  PROP_UUID,
  PROP_DESCRIPTION
};

#define DEFAULT_ENABLED FALSE
#define DEFAULT_STATE GSS_PROGRAM_STATE_STOPPED
#define DEFAULT_UUID "00000000-0000-0000-0000-000000000000"
#define DEFAULT_DESCRIPTION ""


static void gss_program_get_resource (GssTransaction * transaction);
static void gss_program_frag_resource (GssTransaction * transaction);
static void gss_program_list_resource (GssTransaction * transaction);
static void gss_program_png_resource (GssTransaction * transaction);
static void gss_program_jpeg_resource (GssTransaction * transaction);

static void gss_program_finalize (GObject * object);
static void gss_program_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gss_program_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gss_program_add_resources (GssProgram * program);

static GObjectClass *parent_class;

G_DEFINE_TYPE (GssProgram, gss_program, GSS_TYPE_OBJECT);

static GType
gss_program_state_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {GSS_PROGRAM_STATE_UNKNOWN, "unknown", "unknown"},
    {GSS_PROGRAM_STATE_STOPPED, "stopped", "stopped"},
    {GSS_PROGRAM_STATE_STARTING, "starting", "starting"},
    {GSS_PROGRAM_STATE_RUNNING, "running", "running"},
    {GSS_PROGRAM_STATE_STOPPING, "stopping", "stopping"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GssProgramState", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

const char *
gss_program_state_get_name (GssProgramState state)
{
  GEnumValue *ev;

  ev = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (gss_program_state_get_type ())), state);
  if (ev == NULL)
    return NULL;

  return ev->value_name;
}

static void
gss_program_init (GssProgram * program)
{
  guint8 uuid[16];

  program->metrics = gss_metrics_new ();

  program->enable_streaming = TRUE;

  program->state = DEFAULT_STATE;
  program->enabled = DEFAULT_ENABLED;
  gss_uuid_create (uuid);
  program->uuid = gss_uuid_to_string (uuid);
  program->description = g_strdup (DEFAULT_DESCRIPTION);
}

static void
gss_program_class_init (GssProgramClass * program_class)
{
  G_OBJECT_CLASS (program_class)->set_property = gss_program_set_property;
  G_OBJECT_CLASS (program_class)->get_property = gss_program_get_property;
  G_OBJECT_CLASS (program_class)->finalize = gss_program_finalize;

  g_object_class_install_property (G_OBJECT_CLASS (program_class),
      PROP_ENABLED, g_param_spec_boolean ("enabled", "Enabled",
          "Enabled", DEFAULT_ENABLED,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (program_class),
      PROP_STATE, g_param_spec_enum ("state", "State",
          "State", gss_program_state_get_type (), DEFAULT_STATE,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (program_class),
      PROP_UUID, g_param_spec_string ("uuid", "UUID",
          "Unique Identifier", DEFAULT_UUID,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (program_class),
      PROP_DESCRIPTION, g_param_spec_string ("description", "Description",
          "Description", DEFAULT_DESCRIPTION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  program_class->add_resources = gss_program_add_resources;

  parent_class = g_type_class_peek_parent (program_class);
}

static void
gss_program_finalize (GObject * object)
{
  GssProgram *program = GSS_PROGRAM (object);

  gss_program_stop (program);

  g_list_free_full (program->streams, g_object_unref);

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
  g_free (program->description);
  g_free (program->uuid);

  parent_class->finalize (object);
}

static void
gss_program_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GssProgram *program;

  program = GSS_PROGRAM (object);

  switch (prop_id) {
    case PROP_ENABLED:
      gss_program_set_enabled (program, g_value_get_boolean (value));
      break;
    case PROP_DESCRIPTION:
      g_free (program->description);
      program->description = g_value_dup_string (value);
      break;
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

  switch (prop_id) {
    case PROP_ENABLED:
      g_value_set_boolean (value, program->enabled);
      break;
    case PROP_STATE:
      g_value_set_enum (value, program->state);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, program->description);
      break;
    case PROP_UUID:
      g_value_set_string (value, program->uuid);
      break;
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

static void
gss_program_add_resources (GssProgram * program)
{
  char *s;

  s = g_strdup_printf ("/%s", GSS_OBJECT_NAME (program));
  program->resource =
      gss_server_add_resource (GSS_OBJECT_SERVER (program), s, GSS_RESOURCE_UI,
      GSS_TEXT_HTML, gss_program_get_resource, NULL, gss_config_post_resource,
      program);
  g_free (s);

  s = g_strdup_printf ("/%s.frag", GSS_OBJECT_NAME (program));
  gss_server_add_resource (GSS_OBJECT_SERVER (program), s, GSS_RESOURCE_UI,
      GSS_TEXT_PLAIN, gss_program_frag_resource, NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s.list", GSS_OBJECT_NAME (program));
  gss_server_add_resource (GSS_OBJECT_SERVER (program), s, GSS_RESOURCE_UI,
      GSS_TEXT_PLAIN, gss_program_list_resource, NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s-snapshot.png", GSS_OBJECT_NAME (program));
  gss_server_add_resource (GSS_OBJECT_SERVER (program), s, GSS_RESOURCE_UI,
      "image/png", gss_program_png_resource, NULL, NULL, program);
  g_free (s);

  s = g_strdup_printf ("/%s-snapshot.jpeg", GSS_OBJECT_NAME (program));
  gss_server_add_resource (GSS_OBJECT_SERVER (program), s, 0,
      "image/jpeg", gss_program_jpeg_resource, NULL, NULL, program);
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
  g_return_if_fail (GSS_IS_PROGRAM (program));
  g_return_if_fail (GSS_IS_STREAM (stream));

  program->streams = g_list_append (program->streams, stream);

  stream->program = program;
  gss_stream_add_resources (stream);
}

void
gss_program_remove_stream (GssProgram * program, GssStream * stream)
{
  g_return_if_fail (GSS_IS_PROGRAM (program));
  g_return_if_fail (GSS_IS_STREAM (stream));

  program->streams = g_list_remove (program->streams, stream);

  gss_stream_remove_resources (stream);
  stream->program = NULL;

  g_object_unref (stream);
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

static gboolean
idle_state_enable (gpointer ptr)
{
  GssProgram *program = GSS_PROGRAM (ptr);
  gboolean enabled;

  program->state_idle = 0;

  enabled = (program->enabled && GSS_OBJECT_SERVER (program)->enable_programs);
  if (program->state == GSS_PROGRAM_STATE_STOPPED && enabled) {
    gss_program_start (program);
  } else if (program->state == GSS_PROGRAM_STATE_RUNNING && !enabled) {
    gss_program_stop (program);
  }

  return FALSE;
}

void
gss_program_set_state (GssProgram * program, GssProgramState state)
{
  gboolean enabled;

  enabled = (program->enabled && GSS_OBJECT_SERVER (program)->enable_programs);
  program->state = state;
  if ((program->state == GSS_PROGRAM_STATE_STOPPED && enabled) ||
      (program->state == GSS_PROGRAM_STATE_RUNNING && !enabled)) {
    if (!program->state_idle) {
      program->state_idle = g_idle_add (idle_state_enable, program);
    }
  }
}

void
gss_program_set_enabled (GssProgram * program, gboolean enabled)
{
  if (program->enabled && !enabled) {
    program->enabled = enabled;
    gss_program_stop (program);
  } else if (!program->enabled && enabled) {
    program->enabled = enabled;
    gss_program_start (program);
  }
}

void
gss_program_stop (GssProgram * program)
{
  GssProgramClass *program_class;

  if (program->state == GSS_PROGRAM_STATE_STOPPED ||
      program->state == GSS_PROGRAM_STATE_STOPPING) {
    return;
  }
  GST_DEBUG_OBJECT (program, "stop");
  gss_program_set_state (program, GSS_PROGRAM_STATE_STOPPING);

  program_class = GSS_PROGRAM_GET_CLASS (program);
  if (program_class->stop) {
    program_class->stop (program);
  } else {
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

#if 0
    for (g = program->streams; g; g = g_list_next (g)) {
      GssStream *stream = g->data;
      g_object_unref (stream);
    }
#endif
  }
}

void
gss_program_start (GssProgram * program)
{
  GssProgramClass *program_class;
  GList *g;

  if (program->state == GSS_PROGRAM_STATE_STARTING ||
      program->state == GSS_PROGRAM_STATE_RUNNING ||
      program->state == GSS_PROGRAM_STATE_STOPPING) {
    return;
  }
  if (!program->enabled || !GSS_OBJECT_SERVER (program)->enable_programs) {
    return;
  }
  GST_DEBUG_OBJECT (program, "start");
  gss_program_set_state (program, GSS_PROGRAM_STATE_STARTING);

  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = GSS_STREAM (g->data);
    gss_stream_add_resources (stream);
  }

  program_class = GSS_PROGRAM_GET_CLASS (program);
  if (program_class->start) {
    program_class->start (program);
  }
}

GssStream *
gss_program_get_stream (GssProgram * program, int index)
{
  return (GssStream *) g_list_nth_data (program->streams, index);
}

int
gss_program_get_stream_index (GssProgram * program, GssStream * stream)
{
  GList *g;
  int index = 0;
  for (g = program->streams; g; g = g->next) {
    if (g->data == stream)
      return index;
    index++;
  }

  return -1;
}

int
gss_program_get_n_streams (GssProgram * program)
{
  return g_list_length (program->streams);
}

void
gss_program_set_jpegsink (GssProgram * program, GstElement * jpegsink)
{
  if (program->jpegsink)
    g_object_unref (program->jpegsink);
  if (jpegsink)
    g_object_ref (jpegsink);
  program->jpegsink = jpegsink;
}

void
gss_program_add_jpeg_block (GssProgram * program, GssTransaction * t)
{
  GString *s = t->s;

  if (program->state == GSS_PROGRAM_STATE_RUNNING) {
    if (program->jpegsink) {
      GSS_P ("<img id='id%d' src='/%s-snapshot.jpeg' alt='snapshot'>",
          t->id, GSS_OBJECT_NAME (program));
      if (t->script == NULL)
        t->script = g_string_new ("");
      g_string_append_printf (t->script,
          "$(document).ready(function() {\n"
          "document.getElementById('id%d').src="
          "'/%s-snapshot.jpeg?_=' + new Date().getTime();\n"
          "var refreshId = setInterval(function() {\n"
          "document.getElementById('id%d').src="
          "'/%s-snapshot.jpeg?_=' + new Date().getTime();\n"
          " }, 1000);\n"
          "});\n",
          t->id, GSS_OBJECT_NAME (program), t->id, GSS_OBJECT_NAME (program));
      t->id++;
    } else {
      GSS_P ("<img src='/no-snapshot.png' alt='no snapshot'>\n");
    }
  } else {
    GSS_P ("<img src='/offline.png' alt='offline'>\n");
  }
}

void
gss_program_add_video_block (GssProgram * program, GssTransaction * t,
    int max_width)
{
  GString *s = t->s;
  GList *g;
  int width = 0;
  int height = 0;
  int flash_only = TRUE;

  if (program->state != GSS_PROGRAM_STATE_RUNNING) {
    GSS_P ("<img src='/offline.png' alt='offline'>\n");
    return;
  }

  if (program->streams == NULL) {
    if (program->jpegsink) {
      gss_html_append_image_printf (s,
          "/%s-snapshot.jpeg", 0, 0, "snapshot image",
          GSS_OBJECT_NAME (program));
    } else {
      GSS_P ("<img src='/no-snapshot.png' alt='no snapshot'>\n");
    }
  }

  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;
    if (stream->width > width)
      width = stream->width;
    if (stream->height > height)
      height = stream->height;
    if (stream->type != GSS_STREAM_TYPE_FLV_H264BASE_AAC) {
      flash_only = FALSE;
    }
  }
  if (max_width != 0 && width > max_width) {
    height = max_width * 9 / 16;
    width = max_width;
  }

  if (GSS_OBJECT_SERVER (program)->enable_html5_video && !flash_only) {
    GSS_P ("<video controls=\"controls\" autoplay=\"autoplay\" "
        "id=video width=\"%d\" height=\"%d\">\n", width, height);

    for (g = g_list_last (program->streams); g; g = g_list_previous (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_WEBM) {
        GSS_P
            ("<source src=\"%s\" type='video/webm; codecs=\"vp8, vorbis\"'>\n",
            stream->location);
      }
    }

    for (g = g_list_last (program->streams); g; g = g_list_previous (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_OGG_THEORA_VORBIS) {
        GSS_P
            ("<source src=\"%s\" type='video/ogg; codecs=\"theora, vorbis\"'>\n",
            stream->location);
      }
    }

    for (g = g_list_last (program->streams); g; g = g_list_previous (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_M2TS_H264BASE_AAC ||
          stream->type == GSS_STREAM_TYPE_M2TS_H264MAIN_AAC) {
        GSS_P ("<source src=\"/%s.m3u8\" >\n", GSS_OBJECT_NAME (program));
        break;
      }
    }

  }

  if (GSS_OBJECT_SERVER (program)->enable_cortado) {
    for (g = program->streams; g; g = g_list_next (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_OGG_THEORA_VORBIS) {
        GSS_P ("<applet code=\"com.fluendo.player.Cortado.class\"\n"
            "  archive=\"/cortado.jar\" width=\"%d\" height=\"%d\">\n"
            "    <param name=\"url\" value=\"%s\"></param>\n"
            "</applet>\n", width, height, stream->location);
        break;
      }
    }
  }

  if (GSS_OBJECT_SERVER (program)->enable_flash) {
    for (g = program->streams; g; g = g_list_next (g)) {
      GssStream *stream = g->data;
      if (stream->type == GSS_STREAM_TYPE_FLV_H264BASE_AAC) {
        if (t->server->enable_osplayer) {
          GSS_P (" <object width='%d' height='%d' id='flvPlayer' "
              "type=\"application/x-shockwave-flash\" "
              "data=\"OSplayer.swf\">\n"
              "  <param name='allowFullScreen' value='true'>\n"
              "  <param name=\"allowScriptAccess\" value=\"always\"> \n"
              "  <param name=\"movie\" value=\"OSplayer.swf\"> \n"
              "  <param name=\"flashvars\" value=\""
              "movie=%s"
              "&btncolor=0x333333"
              "&accentcolor=0x31b8e9"
              "&txtcolor=0xdddddd"
              "&volume=30"
              "&autoload=on"
              "&autoplay=off"
              "&vTitle=TITLE"
              "&showTitle=yes\">\n", width, height + 24, stream->location);
          if (program->enable_snapshot) {
            gss_html_append_image_printf (s,
                "/%s-snapshot.png", 0, 0, "snapshot image",
                GSS_OBJECT_NAME (program));
          }
          GSS_P (" </object>\n");
        } else if (t->server->enable_flowplayer) {
          GSS_P
              ("<a href='%s' style='display:block;width:%dpx;height:%dpx' id='player'>\n",
              stream->location, width, height);
          GSS_P ("Click to play video.\n");
#if 0
          if (program->enable_snapshot) {
            gss_html_append_image_printf (s,
                "/%s-snapshot.png", 0, 0, "snapshot image",
                GSS_OBJECT_NAME (program));
          }
#endif
          GSS_P ("</a>\n");
        }
        break;
      }

    }
  } else {
    if (program->enable_snapshot) {
      gss_html_append_image_printf (s,
          "/%s-snapshot.png", 0, 0, "snapshot image",
          GSS_OBJECT_NAME (program));
    }
  }

  if (GSS_OBJECT_SERVER (program)->enable_html5_video && !flash_only) {
    GSS_A ("</video>\n");
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
  gss_program_add_video_block (program, t, 0);
}

static void
gss_program_get_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GString *s = g_string_new ("");

  t->s = s;

  gss_html_header (t);

  GSS_P ("<h1>%s</h1>\n", GSS_OBJECT_SAFE_TITLE (program));

  gss_program_add_video_block (program, t, 0);

  GSS_A ("<br>");

  gss_program_add_stream_table (program, s);

  if (t->session && t->session->is_admin) {
    gss_config_append_config_block (G_OBJECT (program), t, FALSE);
  }

  gss_html_footer (t);
}

void
gss_program_add_stream_table (GssProgram * program, GString * s)
{
  GList *g;
  gboolean have_hls = FALSE;

  GSS_A ("<table class='table table-striped table-bordered "
      "table-condensed'>\n");
  GSS_A ("<thead>\n");
  GSS_A ("<tr>\n");
  GSS_A ("<th>Type</th>\n");
  GSS_A ("<th>Size</th>\n");
  GSS_A ("<th>Bitrate</th>\n");
  GSS_A ("<th></th>\n");
  GSS_A ("<th></th>\n");
  GSS_A ("</tr>\n");
  GSS_A ("</thead>\n");
  GSS_A ("<tbody>\n");
  for (g = program->streams; g; g = g_list_next (g)) {
    GssStream *stream = g->data;

    GSS_A ("<tr>\n");
    GSS_P ("<td>%s</td>\n", gss_stream_type_get_name (stream->type));
    GSS_P ("<td>%dx%d</td>\n", stream->width, stream->height);
    GSS_P ("<td>%d kbps</td>\n", stream->bitrate / 1000);
    GSS_P ("<td><a href=\"%s\">stream</a></td>\n", stream->location);
    GSS_P ("<td><a href=\"%s\">playlist</a></td>\n", stream->playlist_location);
    GSS_A ("</tr>\n");

    if (stream->type == GSS_STREAM_TYPE_M2TS_H264BASE_AAC ||
        stream->type == GSS_STREAM_TYPE_M2TS_H264MAIN_AAC) {
      have_hls = TRUE;
    }
  }
  if (have_hls) {
    GSS_A ("<tr>\n");
    GSS_P ("<td colspan='5'><a href='/%s.m3u8'>HLS</a></td>\n",
        GSS_OBJECT_NAME (program));
    GSS_A ("</tr>\n");
  }
  GSS_A ("<tr>\n");
  GSS_P ("<td colspan='5'><a class='btn btn-mini' href='/'>"
      "<i class='icon-plus'></i>Add</a></td>\n");
  GSS_A ("</tr>\n");
  GSS_A ("</tbody>\n");
  GSS_A ("</table>\n");

}


static void
gss_program_list_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GString *s = g_string_new ("");
  GList *g;
  int i = 0;

  t->s = s;

  for (g = program->streams; g; g = g_list_next (g), i++) {
    GssStream *stream = g->data;
    GSS_P ("%d %s %d %d %d %s\n", i, gss_stream_type_get_id (stream->type),
        stream->width, stream->height, stream->bitrate, stream->location);
  }
}

static void
gss_program_png_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GstBuffer *buffer = NULL;

  if (!program->enable_streaming || program->state != GSS_PROGRAM_STATE_RUNNING) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (program->pngappsink) {
    g_object_get (program->pngappsink, "last-buffer", &buffer, NULL);
  }

  if (buffer) {
#if GST_CHECK_VERSION(1,0,0)
    GstMapInfo mapinfo;

    soup_message_set_status (t->msg, SOUP_STATUS_OK);

    gst_buffer_map (buffer, &mapinfo, GST_MAP_READ);
    soup_message_set_response (t->msg, "image/png", SOUP_MEMORY_COPY,
        (char *) mapinfo.data, mapinfo.size);

    gst_buffer_unmap (buffer, &mapinfo);
#else
    soup_message_set_status (t->msg, SOUP_STATUS_OK);

    soup_message_set_response (t->msg, "image/png", SOUP_MEMORY_COPY,
        (char *) GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));
#endif

    gst_buffer_unref (buffer);
  } else {
    gss_html_error_404 (t->server, t->msg);
  }

}

static void
gss_program_jpeg_resource (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GstBuffer *buffer = NULL;

  if (!program->enable_streaming || program->state != GSS_PROGRAM_STATE_RUNNING) {
    soup_message_set_status (t->msg, SOUP_STATUS_NO_CONTENT);
    return;
  }

  if (program->jpegsink) {
    g_object_get (program->jpegsink, "last-buffer", &buffer, NULL);
  }

  if (buffer) {
#if GST_CHECK_VERSION(1,0,0)
    GstMapInfo mapinfo;

    soup_message_set_status (t->msg, SOUP_STATUS_OK);

    gst_buffer_map (buffer, &mapinfo, GST_MAP_READ);
    soup_message_set_response (t->msg, "image/jpeg", SOUP_MEMORY_COPY,
        (char *) mapinfo.data, mapinfo.size);

    gst_buffer_unmap (buffer, &mapinfo);
#else
    soup_message_set_status (t->msg, SOUP_STATUS_OK);

    soup_message_set_response (t->msg, "image/jpeg", SOUP_MEMORY_COPY,
        (char *) GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));
#endif

    gst_buffer_unref (buffer);
  } else {
    gss_html_error_404 (t->server, t->msg);
  }

}
