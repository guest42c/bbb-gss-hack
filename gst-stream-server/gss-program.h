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


#ifndef _GSS_PROGRAM_H
#define _GSS_PROGRAM_H

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <libsoup/soup.h>
#include "gss-config.h"
#include "gss-types.h"
#include "gss-session.h"

G_BEGIN_DECLS


typedef enum {
  GSS_PROGRAM_EW_FOLLOW,
  GSS_PROGRAM_HTTP_FOLLOW,
  GSS_PROGRAM_HTTP_PUT,
  GSS_PROGRAM_EW_CONTRIB,
  GSS_PROGRAM_ICECAST,
  GSS_PROGRAM_MANUAL
} GssProgramType;

struct _GssProgram {
  GssServer *server;

  GssProgramType program_type;
  gboolean is_archive;

  char *location;
  char *follow_uri;
  char *follow_host;

  SoupClientContext *push_client;
  int push_media_type;

  gboolean running;
  gboolean enable_streaming;

  int n_streams;
  GssServerStream **streams;
  GssMetrics *metrics;

  gboolean enable_ogv;
  gboolean enable_webm;
  gboolean enable_hls;
  gboolean enable_snapshot;
  int restart_delay;

  GstElement *pngappsink;
  GstElement *jpegsink;

  int n_hls_chunks;
  struct {
    SoupBuffer *variant_buffer; /* contents of current variant file */

    int target_duration; /* max length of a chunk (in seconds) */
    gboolean is_encrypted;
    const char *key_uri;
    gboolean have_iv;
    guint32 init_vector[4];
  } hls;
};


GssProgram * gss_program_new (const char *program_name);
void gss_program_add_server_resources (GssProgram *program);
void gss_program_remove_server_resources (GssProgram *program);
void gss_program_free (GssProgram *program);
void gss_program_set_jpegsink (GssProgram *program, GstElement *jpegsink);
void gss_program_stop (GssProgram * program);
void gss_program_start (GssProgram * program);

void gss_program_follow (GssProgram *program, const char *host,
    const char *stream);
void gss_program_http_follow (GssProgram *progra, const char *uri);
void gss_program_ew_contrib (GssProgram *program);
void gss_program_http_put (GssProgram *program);
void gss_program_icecast (GssProgram *program);
void gss_program_follow_get_list (GssProgram *program);
GssServerStream * gss_program_add_ogv_stream (GssProgram *program);
GssServerStream * gss_program_add_webm_stream (GssProgram *program);
GssServerStream * gss_program_add_hls_stream (GssProgram *program);
void gss_program_add_stream (GssProgram *program, GssServerStream *stream);
GssServerStream * gss_program_add_stream_full (GssProgram *program,
    int type, int width, int height, int bitrate, GstElement *sink);
void gss_program_log (GssProgram *program, const char *message, ...);
void gss_program_enable_streaming (GssProgram *program);
void gss_program_disable_streaming (GssProgram *program);
void gss_program_set_running (GssProgram *program, gboolean running);


void gss_program_add_video_block (GssProgram *program, GString *s, int max_width,
    const char *base_url);

/* FIXME move to program-follow */
void
gss_program_add_stream_follow (GssProgram * program, int type, int width,
    int height, int bitrate, const char *url);


G_END_DECLS

#endif

