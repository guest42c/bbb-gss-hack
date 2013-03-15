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

#include "gss-types.h"


G_BEGIN_DECLS

struct _GssServer;
struct _GssAddrRangeList;
typedef struct _GssAddrRangeList GssAddrRangeList;

struct _GssSession {
  gint refcount;
  char *session_id;
  char *username;
  time_t last_time;
  gboolean permanent;
  gboolean valid;
  gboolean is_admin;
  gpointer priv;
};

typedef gpointer (*GssSessionAuthorizationFunc) (GssSession *session,
    gpointer priv);

GssSession * gss_session_new (const char *username);
GssSession * gss_session_ref (GssSession *session);
GList * gss_session_get_list (void);
void gss_session_invalidate (GssSession *session);
void gss_session_unref (GssSession *session);
void gss_session_add_session_callbacks (struct _GssServer * server);
void gss_session_set_hosts_allow (GssServer *server);
gboolean gss_addr_address_check (GssServer *server, SoupClientContext * context);
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
gboolean gss_session_is_valid (GssSession * session);
void gss_session_set_authorization_function (GssSessionAuthorizationFunc func,
    gpointer priv);

/* GstAddrRangeList */

void gss_addr_range_list_free (GssAddrRangeList *addr_range_list);
GssAddrRangeList *gss_addr_range_list_new (int n_entries);
GssAddrRangeList *gss_addr_range_list_new_from_string (const char *str,
    gboolean default_all, gboolean allow_localhost);
gboolean gss_addr_range_list_check_address (const GssAddrRangeList
    *addr_range_list, SoupAddress *addr);


G_END_DECLS

#endif

