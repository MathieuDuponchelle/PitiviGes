/* GStreamer Editing Services
 * Copyright (C) 2009 Edward Hervey <bilboed@bilboed.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <ges/ges.h>

int
main (int argc, gchar ** argv)
{
  GESPipeline *pipeline;
  GESTimeline *timeline;
  GESTrack *tracka;
  GESLayer *layer;
  GMainLoop *mainloop;
  GstClockTime offset = 0;
  guint i;

  if (argc < 2) {
    g_print ("Usage: %s <list of audio files>\n", argv[0]);
    return -1;
  }

  /* Initialize GStreamer (this will parse environment variables and commandline
   * arguments. */
  gst_init (&argc, &argv);

  /* Initialize the GStreamer Editing Services */
  ges_init ();

  /* Setup of an audio timeline */

  /* This is our main GESTimeline */
  timeline = ges_timeline_new ();

  tracka = GES_TRACK (ges_audio_track_new ());

  /* We are only going to be doing one layer of clips */
  layer = ges_layer_new ();

  /* Add the tracks and the layer to the timeline */
  if (!ges_timeline_add_layer (timeline, layer))
    return -1;
  if (!ges_timeline_add_track (timeline, tracka))
    return -1;

  /* Here we've finished initializing our timeline, we're 
   * ready to start using it... by solely working with the layer ! */

  for (i = 1; i < argc; i++, offset += GST_SECOND) {
    gchar *uri = gst_filename_to_uri (argv[i], NULL);
    GESUriClip *src = ges_uri_clip_new (uri);

    g_assert (src);
    g_free (uri);

    g_object_set (src, "start", offset, "duration", GST_SECOND, NULL);

    ges_layer_add_clip (layer, (GESClip *) src);
  }

  /* In order to listen our timeline, let's grab a convenience pipeline to put
   * our timeline in. */
  pipeline = ges_pipeline_new ();

  /* Add the timeline to that pipeline */
  if (!ges_pipeline_set_timeline (pipeline, timeline))
    return -1;

  /* The following is standard usage of a GStreamer pipeline (note how you
   * haven't had to care about GStreamer so far ?).
   *
   * We set the pipeline to playing ... */
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);

  /* ... and we start a GMainLoop. GES **REQUIRES** a GMainLoop to be running in
   * order to function properly ! */
  mainloop = g_main_loop_new (NULL, FALSE);

  /* Simple code to have the mainloop shutdown after 4s */
  g_timeout_add_seconds (argc - 1, (GSourceFunc) g_main_loop_quit, mainloop);
  g_main_loop_run (mainloop);

  return 0;
}
