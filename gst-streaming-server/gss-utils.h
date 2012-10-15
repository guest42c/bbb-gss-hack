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


#ifndef _GSS_UTILS_H
#define _GSS_UTILS_H

#include <glib.h>
#include <glib-object.h>

#include <string.h>

G_BEGIN_DECLS

char * gss_utils_get_time_string (void);
char * gss_utils_get_ip_address_string (const char *interface);
char * gss_utils_gethostname (void);
void gss_utils_dump_hash (GHashTable *hash);
void gss_utils_get_random_bytes (guint8 *entropy, int n);
char * g_object_get_as_string (GObject * object, const GParamSpec * pspec);
gboolean g_object_set_as_string (GObject * obj, const char *property,
    const char *value);
gboolean g_object_property_is_default (GObject * object,
    const GParamSpec * pspec);
char * gss_utils_crlf_to_lf (const char *s);
gboolean gss_object_param_is_secure (GObject *object, const char *property_name);
void gss_uuid_create (guint8 * uuid);
char * gss_uuid_to_string (guint8 * uuid);

  
G_END_DECLS

#endif

