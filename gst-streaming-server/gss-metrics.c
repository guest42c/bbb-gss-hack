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


GssMetrics *
gss_metrics_new (void)
{
  GssMetrics *metrics;

  metrics = g_new0 (GssMetrics, 1);

  return metrics;
}

void
gss_metrics_free (GssMetrics * metrics)
{
  g_free (metrics);
}

void
gss_metrics_add_client (GssMetrics * metrics, int bitrate)
{
  metrics->n_clients++;
  metrics->max_clients = MAX (metrics->max_clients, metrics->n_clients);

  metrics->bitrate += bitrate;
  metrics->max_bitrate = MAX (metrics->max_bitrate, metrics->bitrate);

}

void
gss_metrics_remove_client (GssMetrics * metrics, int bitrate)
{
  metrics->n_clients--;
  metrics->bitrate -= bitrate;
}
