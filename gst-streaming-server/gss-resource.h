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


#ifndef _GSS_RESOURCE_H
#define _GSS_RESOURCE_H

#include "gss-config.h"
#include "gss-types.h"
#include "gss-transaction.h"

G_BEGIN_DECLS


typedef enum {
  GSS_RESOURCE_ADMIN = (1<<0),
  GSS_RESOURCE_UI = (1<<1),
  GSS_RESOURCE_HTTP_ONLY = (1<<2),
  GSS_RESOURCE_HTTPS_ONLY = (1<<3),
  GSS_RESOURCE_ONETIME = (1<<4),
  GSS_RESOURCE_USER = (1<<5),
  GSS_RESOURCE_KIOSK = (1<<6),
} GssResourceFlags;

struct _GssResource {
  char *location;
  char *etag;
  char *name;
  const char *content_type;

  GssResourceFlags flags;

  GssTransactionCallback *get_callback;
  GssTransactionCallback *put_callback;
  GssTransactionCallback *post_callback;

  GDestroyNotify destroy;

  gpointer priv;
};


void gss_resource_unimplemented (GssTransaction * t);
void gss_resource_file (GssTransaction * transaction);
void gss_resource_onetime (GssTransaction * t);

void gss_resource_free (GssResource * resource);

void gss_resource_onetime_redirect (GssTransaction *t);

GssResource * gss_resource_new_file (const char *filename, GssResourceFlags flags,
    const char *content_type);
GssResource * gss_resource_new_static (const char *filename,
    GssResourceFlags flags, const char *content_type, const char *string,
    int len);
GssResource * gss_resource_new_string (const char *filename,
    GssResourceFlags flags, const char *content_type, const char *string);


G_END_DECLS

#endif

