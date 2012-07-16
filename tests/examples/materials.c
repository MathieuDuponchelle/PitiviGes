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

static void
material_loaded (GObject * source_object, GAsyncResult * res,
    gpointer user_data)
{
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

  ges_material_source_new (argv[1], material_loaded, NULL);

  g_main_loop_run (mainloop);

  return 0;
}
