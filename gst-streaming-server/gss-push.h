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


#ifndef _GSS_PUSH_H
#define _GSS_PUSH_H

#include <gst/gst.h>
#include "gss-program.h"

G_BEGIN_DECLS

#define GSS_TYPE_PUSH \
  (gss_push_get_type())
#define GSS_PUSH(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GSS_TYPE_PUSH,GssPush))
#define GSS_PUSH_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GSS_TYPE_PUSH,GssPushClass))
#define GSS_PUSH_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSS_TYPE_PUSH, GssPushClass))
#define GSS_IS_PUSH(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GSS_TYPE_PUSH))
#define GSS_IS_PUSH_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GSS_TYPE_PUSH))

typedef enum {
  GSS_PUSH_METHOD_HTTP_PUT,
  GSS_PUSH_METHOD_TCP_SOCKET,
  GSS_PUSH_METHOD_ICECAST
} GssPushMethod;

typedef struct _GssPush GssPush;
typedef struct _GssPushClass GssPushClass;

struct _GssPush {
  GssProgram program;

  /* properties */
  char *push_uri;
  GssPushMethod push_method;

  SoupClientContext *push_client;
  int push_media_type;
};

struct _GssPushClass
{
  GssProgramClass program_class;

};

GType gss_push_get_type (void);

GssProgram *gss_push_new (void);


G_END_DECLS

#endif

