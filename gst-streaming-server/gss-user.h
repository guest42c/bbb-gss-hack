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

#ifndef _GSS_USER_H_
#define _GSS_USER_H_

#include <gst/gst.h>

#include "gss-server.h"

#define GSS_TYPE_USER \
  (gss_user_get_type())
#define GSS_USER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GSS_TYPE_USER,GssUser))
#define GSS_USER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GSS_TYPE_USER,GssUserClass))
#define GSS_IS_USER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GSS_TYPE_USER))
#define GSS_IS_USER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GSS_TYPE_USER))

enum {
  GSS_USER_GROUP_GSS_ADMIN = (1<<0),
  GSS_USER_GROUP_ADMIN = (1<<1),
  GSS_USER_GROUP_PRODUCER = (1<<2),
  GSS_USER_GROUP_USER = (1<<3)
};

typedef struct _GssUser GssUser;
typedef struct _GssUserClass GssUserClass;

typedef struct _GssUserInfo GssUserInfo;
struct _GssUserInfo {
  char *username;
  guint groups;
  guint requested_groups;
};

struct _GssUser {
  GstObject object;

  /* properties */
  GHashTable *users;
  gchar *admin_password0;
  gchar *admin_password1;
};

struct _GssUserClass {
  GstObjectClass object_class;

};

GType gss_user_get_type (void);

GssUser *gss_user_new (void);
void gss_user_parse_users_string (GssUser *user, const char *s);
char * gss_user_get_string (GssUser *user);
GssUserInfo * gss_user_add_user_info (GssUser *user, const char *username, guint groups);

void gss_user_set_location (GssUser *user, const char *location);

void gss_user_add_resources (GssUser *user, GssServer *server);

gboolean gss_session_is_producer (GssSession *session);

#endif

