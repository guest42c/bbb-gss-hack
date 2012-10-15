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


#ifndef _GSS_MANAGER_H_
#define _GSS_MANAGER_H_

#include <gst/gst.h>
#include <glib/gstdio.h>

#include "gss-server.h"

#define GSS_TYPE_MANAGER \
  (gss_manager_get_type())
#define GSS_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GSS_TYPE_MANAGER,GssManager))
#define GSS_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GSS_TYPE_MANAGER,GssManagerClass))
#define GSS_MANAGER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GSS_TYPE_MANAGER, GssManagerClass))
#define GSS_IS_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GSS_TYPE_MANAGER))
#define GSS_IS_MANAGER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GSS_TYPE_MANAGER))

typedef struct _GssManager GssManager;
typedef struct _GssManagerClass GssManagerClass;

struct _GssManager {
  GstObject object;

  /* properties */
  char *follow_hosts;
};

struct _GssManagerClass {
  GstObjectClass object_class;

};

GType gss_manager_get_type (void);

GssManager *gss_manager_new (void);
void gss_manager_start (GssManager *manager);
void gss_manager_stop (GssManager *manager);
gboolean gss_manager_create_pipeline (GssManager * manager);

void gss_manager_set_location (GssManager *manager, const char *location);

void gss_manager_add_resources (GssManager *manager, GssServer *server);

#endif

