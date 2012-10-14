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


#ifndef _GSS_TRANSACTION_H
#define _GSS_TRANSACTION_H

#include <libsoup/soup.h>
#include "gss-config.h"
#include "gss-types.h"

G_BEGIN_DECLS

typedef void (GssTransactionCallback)(GssTransaction *transaction);

struct _GssTransaction {
  GssServer *server;
  SoupServer *soupserver;
  SoupMessage *msg;
  const char *path;
  GHashTable *query;
  SoupClientContext *client;
  GssResource *resource;
  GssSession *session;
  gboolean done;
  GString *s;
  GString *script;
  int id;
};

void gss_transaction_redirect (GssTransaction * t, const char *target);
void gss_transaction_error (GssTransaction * t, const char *message);


G_END_DECLS

#endif

