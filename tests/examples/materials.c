/* GStreamer Editing Services
 * Copyright (C) 2012 Volodymyr Rudyi<vladimir.rudoy@gmail.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <ges/ges.h>
#include <ges/ges-material-source.h>
#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/gstdiscoverer.h>
#include <ges/ges-internal.h>

static void
material_loaded_cb (GESMaterialFileSource * material, GAsyncResult * res,
    gpointer user_data)
{
  GstDiscovererInfo *discoverer_info = NULL;
  discoverer_info = ges_material_filesource_get_info (material);

  GST_DEBUG ("Result is %d", gst_discoverer_info_get_result (discoverer_info));
  GST_DEBUG ("Info type is %s", G_OBJECT_TYPE_NAME (material));
  GST_DEBUG ("Duration is %lu",
      gst_discoverer_info_get_duration (discoverer_info));
}

int
main (int argc, gchar ** argv)
{
  GMainLoop *mainloop;

  if (argc != 2) {
    return 1;
  }
  /* Initialize GStreamer (this will parse environment variables and commandline
   * arguments. */
  gst_init (&argc, &argv);

  /* Initialize the GStreamer Editing Services */
  ges_init ();

  /* ... and we start a GMainLoop. GES **REQUIRES** a GMainLoop to be running in
   * order to function properly ! */
  mainloop = g_main_loop_new (NULL, FALSE);

  ges_material_new (GES_TYPE_TIMELINE_FILE_SOURCE, NULL,
      (GAsyncReadyCallback) material_loaded_cb, NULL, "uri", argv[1], NULL);

  g_main_loop_run (mainloop);

  return 0;
}
