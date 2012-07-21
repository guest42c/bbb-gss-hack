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


#ifndef _GSS_TYPES_H
#define _GSS_TYPES_H

G_BEGIN_DECLS

typedef struct _GssProgram GssProgram;
typedef struct _GssStream GssStream;
typedef struct _GssServer GssServer;
typedef struct _GssServerClass GssServerClass;
typedef struct _GssConnection GssConnection;
typedef struct _GssHLSSegment GssHLSSegment;
typedef struct _GssRtspStream GssRtspStream;
typedef struct _GssMetrics GssMetrics;
typedef struct _GssResource GssResource;
typedef struct _GssSession GssSession;
typedef struct _GssTransaction GssTransaction;


G_END_DECLS

#endif

