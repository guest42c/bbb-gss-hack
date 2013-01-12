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

#include "config.h"

#include "gss-server.h"
#include "gss-html.h"
#include "gss-resource.h"
#include "gss-soup.h"
#include "gss-utils.h"




void
gss_resource_free (GssResource * resource)
{
  g_free (resource->name);
  g_free (resource->etag);
  if (resource->destroy) {
    resource->destroy (resource);
  }
  g_free (resource);
}

void
gss_resource_unimplemented (GssTransaction * t)
{
  t->s = g_string_new ("");

  gss_html_header (t);

  g_string_append_printf (t->s, "<h1>Unimplemented Feature</h1>\n"
      "<p>The feature \"%s\" is not yet implemented.</p>\n", t->path);

  gss_html_footer (t);
}


/* static resources */

typedef struct _GssStaticResource GssStaticResource;
struct _GssStaticResource
{
  GssResource resource;

  const char *filename;
  const char *contents;
  char *malloc_contents;
  gsize size;
};

void
gss_resource_file (GssTransaction * t)
{
  GssStaticResource *sr = (GssStaticResource *) t->resource;

  soup_message_headers_replace (t->msg->response_headers, "Keep-Alive",
      "timeout=5, max=100");
  soup_message_headers_append (t->msg->response_headers, "Etag",
      sr->resource.etag);
  soup_message_headers_append (t->msg->response_headers, "Cache-Control",
      "max-age=86400");

  soup_message_set_status (t->msg, SOUP_STATUS_OK);

  soup_message_set_response (t->msg, sr->resource.content_type,
      SOUP_MEMORY_STATIC, sr->contents, sr->size);
}

static void
gss_static_resource_destroy (GssStaticResource * sr)
{
  g_free (sr->malloc_contents);
}

static void
generate_etag (GssStaticResource * sr)
{
  GChecksum *checksum;
  gsize n;
  guchar digest[32];

  n = g_checksum_type_get_length (G_CHECKSUM_MD5);
  g_assert (n <= 32);
  checksum = g_checksum_new (G_CHECKSUM_MD5);

  g_checksum_update (checksum, (guchar *) sr->contents, sr->size);
  g_checksum_get_digest (checksum, digest, &n);
  sr->resource.etag = g_base64_encode (digest, n);
  /* remove the trailing = (for MD5) */
  sr->resource.etag[22] = 0;
  g_checksum_free (checksum);
}

GssResource *
gss_resource_new_file (const char *filename, GssResourceFlags flags,
    const char *content_type)
{
  GssStaticResource *sr;
  gsize size = 0;
  char *contents;
  gboolean ret;
  GError *error = NULL;

  ret = g_file_get_contents (filename + 1, &contents, &size, &error);
  if (!ret) {
    g_error_free (error);
    GST_WARNING ("missing file %s", filename);
    return NULL;
  }

  sr = g_new0 (GssStaticResource, 1);

  sr->filename = filename;
  sr->resource.content_type = content_type;
  sr->contents = contents;
  sr->malloc_contents = contents;
  sr->size = size;

  sr->resource.destroy = (GDestroyNotify) gss_static_resource_destroy;
  sr->resource.location = g_strdup (filename);
  sr->resource.flags = flags;
  sr->resource.get_callback = gss_resource_file;
  generate_etag (sr);

  return (GssResource *) sr;
}

GssResource *
gss_resource_new_static (const char *filename,
    GssResourceFlags flags, const char *content_type, const char *string,
    int len)
{
  GssStaticResource *sr;

  sr = g_new0 (GssStaticResource, 1);

  sr->filename = filename;
  sr->resource.content_type = content_type;
  sr->contents = string;
  sr->size = len;
  generate_etag (sr);

  sr->resource.destroy = (GDestroyNotify) gss_static_resource_destroy;
  sr->resource.location = g_strdup (filename);
  sr->resource.flags = flags;
  sr->resource.get_callback = gss_resource_file;

  return (GssResource *) sr;
}

GssResource *
gss_resource_new_string (const char *filename,
    GssResourceFlags flags, const char *content_type, const char *string)
{
  return gss_resource_new_static (filename, flags,
      content_type, string, strlen (string));
}


/* one-time resources */

typedef struct _GssOnetimeResource GssOnetimeResource;
struct _GssOnetimeResource
{
  GssResource resource;

  GssServer *server;
  guint timeout_id;
  GssResource *underlying_resource;
};

static void
onetime_destroy (gpointer priv)
{
  GssOnetimeResource *or = (GssOnetimeResource *) priv;

  g_source_remove (or->timeout_id);
}

static gboolean
onetime_expire (gpointer priv)
{
  GssOnetimeResource *or = (GssOnetimeResource *) priv;

  gss_server_remove_resource (or->server, or->resource.location);

  return FALSE;
}

void
gss_resource_onetime_redirect (GssTransaction * t)
{
  GssOnetimeResource *or;
  char *id;
  char *url;
  char *base_url;

  or = g_new0 (GssOnetimeResource, 1);

  id = gss_session_create_id ();
  or->resource.location = g_strdup_printf ("/%s", id);
  g_free (id);
  or->resource.flags = 0;
  or->resource.get_callback = gss_resource_onetime;
  or->resource.destroy = onetime_destroy;

  or->underlying_resource = t->resource;
  or->server = t->server;
  or->timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 5000,
      onetime_expire, or, NULL);

  g_hash_table_replace (t->server->resources, or->resource.location,
      (GssResource *) or);

  base_url = gss_soup_get_base_url_http (t->server, t->msg);
  url = g_strdup_printf ("%s%s", base_url, or->resource.location);
  g_free (base_url);
  soup_message_headers_append (t->msg->response_headers, "Location", url);
  soup_message_set_status (t->msg, SOUP_STATUS_TEMPORARY_REDIRECT);
  soup_message_set_response (t->msg, GSS_TEXT_PLAIN, SOUP_MEMORY_TAKE,
      url, strlen (url));
}

void
gss_resource_onetime (GssTransaction * t)
{
  GssOnetimeResource *or = (GssOnetimeResource *) t->resource;
  GssResource *r = or->underlying_resource;

  t->resource = or->underlying_resource;
  r->get_callback (t);

  gss_server_remove_resource (t->server, or->resource.location);
}
