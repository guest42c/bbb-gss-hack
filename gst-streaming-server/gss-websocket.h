/* GStreamer
 * Copyright (C) 2012 Jan Schmidt <thaytan@noraisin.net>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GSS_WEBSOCKET_H__
#define __GSS_WEBSOCKET_H__

#include <gst/gst.h>
#include <gst/net/gstnet.h>
#include <libsoup/soup.h>

//#include <src/snra-types.h>

G_BEGIN_DECLS

#define GSS_TYPE_WEBSOCKET (gss_websocket_get_type ())

typedef struct _GssWebsocket GssWebsocket;
typedef struct _GssWebsocketClass GssWebsocketClass;
typedef enum _GssWebsocketType GssWebsocketType;

enum _GssWebsocketType {
  GSS_WEBSOCKET_CHUNKED,
  GSS_WEBSOCKET_WEBSOCKET,
  GSS_WEBSOCKET_SINGLE
};

struct _GssWebsocket
{
  GObject parent;

  GssWebsocketType type;
  gboolean fired_conn_lost;
  gboolean need_body_complete;

  guint conn_id;
  SoupMessage *event_pipe;
  SoupServer *soup;

  /* For talking to websocket clients */
  gint websocket_protocol;

  SoupSocket *socket;
  gchar *host;

  GIOChannel *io;
  guint io_watch;
  gchar *in_buf;
  gchar *in_bufptr;
  gsize in_bufsize;
  gsize in_bufavail;

  gchar *out_buf;
  gsize out_bufsize;

  GList *pending_msgs;

  gulong net_event_sig;
  gulong disco_sig;
  gulong wrote_info_sig;
};

struct _GssWebsocketClass
{
  GObjectClass parent;
};

GType gss_websocket_get_type(void);

GssWebsocket *gss_websocket_new (SoupServer *soup,
    SoupMessage *msg, SoupClientContext *context);
GssWebsocket *gss_websocket_new_single (SoupServer * soup,
    SoupMessage * msg, SoupClientContext * context);

void gss_websocket_send_message (GssWebsocket *client,
  gchar *body, gsize len);

const gchar *gss_websocket_get_host (GssWebsocket *client);

G_END_DECLS

#endif
