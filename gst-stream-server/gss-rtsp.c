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

#include "gss-rtsp.h"
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>




void
gss_server_rtsp_init (GssServer * server)
{
  server->rtsp_server = gst_rtsp_server_new ();
  if (getuid () == 0) {
    gst_rtsp_server_set_service (server->rtsp_server, "554");
  } else {
    gst_rtsp_server_set_service (server->rtsp_server, "8554");
  }
}


GssRtspStream *
gss_rtsp_stream_new (GssServerStream * stream)
{
  GssRtspStream *rtsp_stream;

  rtsp_stream = g_new0 (GssRtspStream, 1);

  rtsp_stream->stream = stream;
  rtsp_stream->server = stream->program->server->rtsp_server;

  return rtsp_stream;
}

void
gss_rtsp_stream_free (GssRtspStream * rtsp_stream)
{

  g_free (rtsp_stream);
}


void
gss_rtsp_stream_start (GssRtspStream * rtsp_stream)
{
  GString *pipe_desc;
  int pipe_fds[2];
  int ret;

  ret = pipe (pipe_fds);
  if (ret < 0) {
    return;
  }

  pipe_desc = g_string_new ("");

  g_string_append (pipe_desc, "( ");
  g_string_append_printf (pipe_desc, "fdsrc fd=%d name=src ! ", pipe_fds[0]);
  g_string_append (pipe_desc, "oggdemux name=demux ! ");
  g_string_append (pipe_desc, "queue ! rtptheorapay name=pay0 pt=96 ");
  g_string_append (pipe_desc, "demux. ! queue ! rtpvorbispay name=pay1 pt=97 ");
  g_string_append (pipe_desc, ")");

  if (verbose)
    printf ("%s\n", pipe_desc->str);

  rtsp_stream->factory = gst_rtsp_media_factory_new ();
  gst_rtsp_media_factory_set_launch (rtsp_stream->factory, pipe_desc->str);
  g_string_free (pipe_desc, FALSE);

  rtsp_stream->mapping =
      gst_rtsp_server_get_media_mapping (rtsp_stream->server);
  gst_rtsp_media_mapping_add_factory (rtsp_stream->mapping, "/stream",
      rtsp_stream->factory);
  g_object_unref (rtsp_stream->mapping);

  g_signal_emit_by_name (rtsp_stream->stream->sink, "add", pipe_fds[1]);

  gst_rtsp_server_attach (rtsp_stream->server, NULL);
}
