/* GStreamer Streaming Server
 * Copyright (C) 2009-2013 Entropy Wave Inc <info@entropywave.com>
 * Copyright (C) 2009-2013 David Schleef <ds@schleef.org>
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


#ifndef _GSS_OBJECT_H
#define _GSS_OBJECT_H

#include "gss-types.h"

G_BEGIN_DECLS

#define GSS_TYPE_OBJECT \
  (gss_object_get_type())
#define GSS_OBJECT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GSS_TYPE_OBJECT,GssObject))
#define GSS_OBJECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GSS_TYPE_OBJECT,GssObjectClass))
#define GSS_IS_OBJECT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GSS_TYPE_OBJECT))
#define GSS_IS_OBJECT_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GSS_TYPE_OBJECT))

struct _GssObject {
  GObject object;

  GssServer *server;
  char *name;
};

#define GSS_OBJECT_NAME(obj) (((GssObject *)(obj))->name)


struct _GssObjectClass {
  GObjectClass object_class;

  void (*attach) (GssObject *object, GssServer *server);
  void (*detach) (GssObject *object);
};


GType gss_object_get_type (void);

void gss_object_set_name (GssObject * object, const char *name);


G_END_DECLS

#endif

