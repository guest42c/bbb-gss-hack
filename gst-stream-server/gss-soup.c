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

#include "gss-html.h"

#include <string.h>
#include <json-glib/json-glib.h>



char *
gss_soup_get_request_host (SoupMessage * msg)
{
  char *host;
  char *colon;
  const char *s;

  s = soup_message_headers_get (msg->request_headers, "Host");
  if (s == NULL)
    return NULL;
  host = strdup (s);
  colon = strchr (host, ':');
  if (colon)
    colon[0] = 0;

  return host;
}

char *
gss_soup_get_base_url_http (GssServer * server, SoupMessage * msg)
{
  char *base_url;
  char *host;

  host = gss_soup_get_request_host (msg);
  if (host == NULL)
    host = g_strdup (server->server_name);

  if (server->port == 80) {
    base_url = g_strdup_printf ("http://%s", host);
  } else {
    base_url = g_strdup_printf ("http://%s:%d", host, server->port);
  }
  g_free (host);

  return base_url;
}

char *
gss_soup_get_base_url_https (GssServer * server, SoupMessage * msg)
{
  char *base_url;
  char *host;

  host = gss_soup_get_request_host (msg);
  if (host == NULL)
    host = g_strdup (server->server_name);

  if (server->https_port == 443) {
    base_url = g_strdup_printf ("https://%s", host);
  } else {
    base_url = g_strdup_printf ("https://%s:%d", host, server->https_port);
  }
  g_free (host);

  return base_url;
}

static void
dump_node (JsonNode * node, int indent)
{
  g_print ("%*snode: %d\n", indent, "", JSON_NODE_TYPE (node));
  switch (JSON_NODE_TYPE (node)) {
    case JSON_NODE_OBJECT:
    {
      GList *g;
      JsonObject *obj = json_node_get_object (node);
      g_print ("%*s  OBJECT\n", indent, "");
      for (g = json_object_get_members (obj); g; g = g_list_next (g)) {
        g_print ("%*s    %s\n", indent, "", (char *) g->data);
        dump_node (json_object_get_member (obj, (char *) g->data), indent + 6);
      }
    }
      break;
    case JSON_NODE_ARRAY:
      g_print ("%*s  ARRAY\n", indent, "");
      break;
    case JSON_NODE_VALUE:
    {
      GValue val = G_VALUE_INIT;
      char *s;
      json_node_get_value (node, &val);
      s = g_strdup_value_contents (&val);
      g_print ("%*s  VALUE %s\n", indent, "", s);
      g_free (s);
      g_value_unset (&val);
    }
      break;
    case JSON_NODE_NULL:
      g_print ("%*s  NULL\n", indent, "");
      break;
    default:
      break;
  }

}

GHashTable *
gss_soup_parse_json (const char *s, int len)
{
  JsonParser *jp;
  JsonNode *node;
  GError *error;

  s = "{\"status\":\"okay\",\"email\":\"ds@schleef.org\",\"audience\":\"localhost\",\"expires\":1337410108556,\"issuer\":\"browserid.org\"}";
  len = strlen (s);

  jp = json_parser_new ();

  json_parser_load_from_data (jp, s, len, &error);

  node = json_parser_get_root (jp);

  dump_node (node, 1);

  return NULL;
}
