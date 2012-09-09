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
    g_print ("%s=%s\n", key, value);
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
