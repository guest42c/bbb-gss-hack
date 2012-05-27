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


#ifndef _GSS_SESSION_H_
#define _GSS_SESSION_H_

#include <glib.h>
#include <libsoup/soup.h>



G_BEGIN_DECLS

struct _GssServer;

typedef struct _GssSession GssSession;
struct _GssSession {
  char *session_id;
  char *username;
  time_t last_time;
  gboolean is_admin;
};

GssSession * gss_session_new (const char *username);
void gss_session_add_session_callbacks (struct _GssServer * server);
void gss_session_notify_hosts_allow (const char *key, void *priv);
gboolean gss_addr_address_check (SoupClientContext *context);
gboolean gss_addr_is_localhost (SoupClientContext *context);
char * gss_session_create_id (void);
GssSession * gss_session_lookup (const char *session_id);
void gss_session_touch (GssSession *session);
GssSession * gss_session_get_session (GHashTable *query);
void gss_session_login_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);
void gss_session_logout_callback (SoupServer *server, SoupMessage *msg,
    const char *path, GHashTable *query, SoupClientContext *client,
    gpointer user_data);


G_END_DECLS

#endif

