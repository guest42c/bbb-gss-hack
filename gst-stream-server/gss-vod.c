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
#include "gss-vod.h"

#include <fcntl.h>


#define verbose FALSE


static void vod_resource_chunked (GssTransaction * transaction);


typedef struct _GssVOD GssVOD;

struct _GssVOD
{
  GssServer *server;
  SoupMessage *msg;
  SoupClientContext *client;

  GstElement *pipeline;
  GstElement *sink;

  int fd;
};



#define SIZE 65536

static void
vod_wrote_chunk (SoupMessage * msg, GssVOD * vod)
{
  char *chunk;
  int len;

  chunk = g_malloc (SIZE);
  len = read (vod->fd, chunk, 65536);
  if (len < 0) {
    g_print ("read error\n");
  }
  if (len == 0) {
    soup_message_body_complete (msg->response_body);
    return;
  }

  soup_message_body_append (msg->response_body, SOUP_MEMORY_TAKE, chunk, len);
}

static void
vod_finished (SoupMessage * msg, GssVOD * vod)
{
  close (vod->fd);
  g_free (vod);
}

static void
vod_resource_chunked (GssTransaction * t)
{
  GssProgram *program = (GssProgram *) t->resource->priv;
  GssVOD *vod;
  char *chunk;
  int len;
  char *s;

  vod = g_malloc0 (sizeof (GssVOD));
  vod->msg = t->msg;
  vod->client = t->client;
  vod->server = t->server;

  s = g_strdup_printf ("%s/%s", t->server->archive_dir,
      GST_OBJECT_NAME (program));
  vod->fd = open (s, O_RDONLY);
  if (vod->fd < 0) {
    g_print ("file not found %s\n", s);
    g_free (s);
    g_free (vod);
    soup_message_set_status (t->msg, SOUP_STATUS_NOT_FOUND);
    return;
  }
  g_free (s);

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_headers_set_encoding (t->msg->response_headers,
      SOUP_ENCODING_CHUNKED);

  chunk = g_malloc (SIZE);
  len = read (vod->fd, chunk, 65536);
  if (len < 0) {
    g_print ("read error\n");
  }

  soup_message_body_append (t->msg->response_body, SOUP_MEMORY_TAKE, chunk,
      len);

  g_signal_connect (t->msg, "wrote-chunk", G_CALLBACK (vod_wrote_chunk), vod);
  g_signal_connect (t->msg, "finished", G_CALLBACK (vod_finished), vod);

}

void
gss_vod_setup (GssServer * server)
{
  GDir *dir;

  dir = g_dir_open (server->archive_dir, 0, NULL);
  if (dir) {
    const gchar *name = g_dir_read_name (dir);

    while (name) {
      if (g_str_has_suffix (name, ".webm")) {
        GssProgram *program;
        GssStream *stream;
        char *s;

        program = gss_server_add_program (server, name);
        program->is_archive = TRUE;

        stream = gss_stream_new (GSS_STREAM_TYPE_WEBM, 640, 360, 600);
        gss_program_add_stream (program, stream);

        s = g_strdup_printf ("%s-%dx%d-%dkbps%s.%s", GST_OBJECT_NAME (program),
            stream->width, stream->height, stream->bitrate / 1000, stream->mod,
            stream->ext);
        gst_object_set_name (GST_OBJECT (stream), s);
        g_free (s);

        s = g_strdup_printf ("/%s", GST_OBJECT_NAME (stream));
        gss_server_add_resource (program->server, s, GSS_RESOURCE_HTTP_ONLY,
            stream->content_type, vod_resource_chunked, NULL, NULL, program);
        g_free (s);
      }
      name = g_dir_read_name (dir);
    }
    g_dir_close (dir);
  }
}
