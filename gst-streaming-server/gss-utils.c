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

#include "gss-utils.h"
#include "gss-config.h"

#include <gst/gst.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>


char *
gss_utils_get_time_string (void)
{
  GDateTime *datetime;
  char *s;

  datetime = g_date_time_new_now_local ();

#if 0
  /* RFC 822 */
  strftime (thetime, 79, "%a, %d %b %y %T %z", tmp);
#endif
  /* RFC 2822 */
  s = g_date_time_format (datetime, "%a, %d %b %Y %H:%M:%S %z");
#if 0
  /* RFC 3339, almost */
  strftime (thetime, 79, "%Y-%m-%d %H:%M:%S%z", tmp);
#endif

  g_date_time_unref (datetime);

  return s;
}

char *
gss_utils_get_ip_address_string (const char *interface)
{
  int sock;
  int ret;
  struct ifreq ifr;

  sock = socket (AF_INET, SOCK_DGRAM, 0);

  memset (&ifr, 0, sizeof (ifr));
  strcpy (ifr.ifr_name, "eth0");

  ret = ioctl (sock, SIOCGIFADDR, &ifr);
  if (ret == 0) {
    struct sockaddr_in *sa = (struct sockaddr_in *) &ifr.ifr_addr;
    guint32 quad = ntohl (sa->sin_addr.s_addr);

    return g_strdup_printf ("%d.%d.%d.%d", (quad >> 24) & 0xff,
        (quad >> 16) & 0xff, (quad >> 8) & 0xff, (quad >> 0) & 0xff);
  }

  return strdup ("127.0.0.1");
}

char *
gss_utils_gethostname (void)
{
  char *s;
  char *t;
  int ret;

  s = g_malloc (1000);
  ret = gethostname (s, 1000);
  if (ret == 0) {
    t = g_strdup (s);
  } else {
    t = gss_utils_get_ip_address_string ("eth0");
  }
  g_free (s);
  return t;
}

void
gss_utils_dump_hash (GHashTable * hash)
{
  GHashTableIter iter;
  char *key, *value;

  g_hash_table_iter_init (&iter, hash);
  while (g_hash_table_iter_next (&iter, (gpointer) & key, (gpointer) & value)) {
    GST_DEBUG ("%s=%s", key, value);
  }
}

static int
get_random_fd (void)
{
  static gsize init = 0;

  if (g_once_init_enter (&init)) {
    int fd = open ("/dev/random", O_RDONLY);

    if (fd < 0) {
      g_warning ("Could not open /dev/random, exiting");
      exit (1);
    }

    g_once_init_leave (&init, fd);
  }

  return (int) init;
}

void
gss_utils_get_random_bytes (guint8 * entropy, int n)
{
  int random_fd = get_random_fd ();
  int i;

  g_return_if_fail (entropy != NULL);
  g_return_if_fail (n > 0);
  g_return_if_fail (n <= 256);

  i = 0;
  while (i < n) {
    fd_set readfds;
    struct timeval timeout;
    int ret;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    FD_ZERO (&readfds);
    FD_SET (random_fd, &readfds);
    ret = select (random_fd + 1, &readfds, NULL, NULL, &timeout);
    if (ret == 0) {
      g_warning
          ("Waited too long to read random bytes.  Please install haveged.");
      exit (1);
    }

    n = read (random_fd, entropy + i, n - i);
    if (n < 0) {
      g_warning ("Error reading /dev/random");
      exit (1);
    }
    i += n;
  }
}

char *
g_object_get_as_string (GObject * object, const GParamSpec * pspec)
{
  GValue value = G_VALUE_INIT;
  char *s;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (object, pspec->name, &value);
  if (G_VALUE_HOLDS_STRING (&value)) {
    s = g_value_dup_string (&value);
  } else if (G_VALUE_HOLDS_ENUM (&value)) {
    const GEnumValue *ev;
    ev = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (pspec->value_type)), g_value_get_enum (&value));
    if (ev) {
      s = g_strdup (ev->value_name);
    } else {
      GST_WARNING ("bad value %d for enum %s", g_value_get_enum (&value),
          g_type_name (pspec->value_type));
      s = g_strdup ("");
    }
  } else {
    s = gst_value_serialize (&value);
  }
  g_value_unset (&value);

  return s;
}

gboolean
g_object_set_as_string (GObject * obj, const char *property, const char *value)
{
  GParamSpec *ps;
  GEnumValue *ev;
  gboolean ret = TRUE;

  g_return_val_if_fail (obj, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (obj), FALSE);

  ps = g_object_class_find_property (G_OBJECT_GET_CLASS (obj), property);
  if (ps == NULL)
    return FALSE;

  if (G_TYPE_IS_ENUM (ps->value_type)) {
    ev = g_enum_get_value_by_name (G_ENUM_CLASS (g_type_class_peek
            (ps->value_type)), value);
    if (ev == NULL)
      return FALSE;

    g_object_set (obj, property, ev->value, NULL);
  } else {
    GValue val = G_VALUE_INIT;
    g_value_init (&val, ps->value_type);
    if (ps->value_type == G_TYPE_STRING) {
      g_value_set_string (&val, value);
    } else {
      ret = gst_value_deserialize (&val, value);
    }
    g_object_set_property (obj, property, &val);
    g_value_reset (&val);
  }

  return ret;
}

gboolean
g_object_property_is_default (GObject * object, const GParamSpec * pspec)
{
  GValue value = G_VALUE_INIT;
  gboolean ret;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (object, pspec->name, &value);

  ret = g_param_value_defaults ((GParamSpec *) pspec, &value);

  g_value_unset (&value);

  return ret;
}

char *
gss_utils_crlf_to_lf (const char *s)
{
  char *t;
  int len;
  int i;
  int j;

  len = strlen (s);
  t = g_malloc (len + 1);

  j = 0;
  for (i = 0; i < len; i++) {
    if (s[i] == '\r' && s[i + 1] == '\n') {
      t[j] = '\n';
      i++;
    } else {
      t[j] = s[i];
    }
    j++;
  }
  t[j] = 0;

  return t;
}

gboolean
gss_object_param_is_secure (GObject * object, const char *property_name)
{
  GParamSpec *ps;

  g_return_val_if_fail (object, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  ps = g_object_class_find_property (G_OBJECT_GET_CLASS (object),
      property_name);
  if (ps == NULL)
    return FALSE;

  if (ps->flags & GSS_PARAM_SECURE)
    return TRUE;
  return FALSE;
}

void
gss_uuid_create (guint8 * uuid)
{
  gss_utils_get_random_bytes (uuid, 16);

  uuid[6] &= 0x0f;
  uuid[6] |= 0x40;
  uuid[8] &= 0x3f;
  uuid[8] |= 0x80;
}

char *
gss_uuid_to_string (guint8 * uuid)
{
  return g_strdup_printf ("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
      "%02x%02x%02x%02x%02x%02x",
      uuid[0],
      uuid[1],
      uuid[2],
      uuid[3],
      uuid[4],
      uuid[5],
      uuid[6],
      uuid[7],
      uuid[8],
      uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}
