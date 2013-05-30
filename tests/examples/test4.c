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
#include <gst/pbutils/encoding-profile.h>

GstEncodingProfile *make_encoding_profile (gchar * audio, gchar * container);

/* This example will take a series of files and create a audio-only timeline
 * containing the first second of each file and render it to the output uri 
 * using ogg/vorbis */

/* make_encoding_profile
 * simple method creating an encoding profile. This is here in
 * order not to clutter the main function. */
GstEncodingProfile *
make_encoding_profile (gchar * audio, gchar * container)
{
  GstEncodingContainerProfile *profile;
  GstEncodingProfile *stream;
  GstCaps *caps;

  caps = gst_caps_from_string (container);
  profile =
      gst_encoding_container_profile_new ((gchar *) "ges-test4", NULL, caps,
      NULL);
  gst_caps_unref (caps);

  caps = gst_caps_from_string (audio);
  stream = (GstEncodingProfile *)
      gst_encoding_audio_profile_new (caps, NULL, NULL, 0);
  gst_encoding_container_profile_add_profile (profile, stream);
  gst_caps_unref (caps);

  return (GstEncodingProfile *) profile;
}

int
main (int argc, gchar ** argv)
{
  GESTimelinePipeline *pipeline;
  GESTimeline *timeline;
  GESTrack *tracka;
  GESLayer *layer;
  GMainLoop *mainloop;
  GstEncodingProfile *profile;
  gchar *container = (gchar *) "application/ogg";
  gchar *audio = (gchar *) "audio/x-vorbis";
  gchar *output_uri;
  guint i;
  GError *err = NULL;
  GOptionEntry options[] = {
    {"format", 'f', 0, G_OPTION_ARG_STRING, &container,
        "Container format", "<GstCaps>"},
    {"aformat", 'a', 0, G_OPTION_ARG_STRING, &audio,
        "Audio format", "<GstCaps>"},
    {NULL}
  };
  GOptionContext *ctx;

  ctx = g_option_context_new ("- renders a sequence of audio files.");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_printerr ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    return -1;
  }

  if (argc < 3) {
    g_print ("Usage: %s <output uri> <list of audio files>\n", argv[0]);
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
  layer = (GESLayer *) ges_simple_layer_new ();

  /* Add the tracks and the layer to the timeline */
  if (!ges_timeline_add_layer (timeline, layer))
    return -1;
  if (!ges_timeline_add_track (timeline, tracka))
    return -1;

  /* Here we've finished initializing our timeline, we're 
   * ready to start using it... by solely working with the layer ! */

  for (i = 2; i < argc; i++) {
    gchar *uri = gst_filename_to_uri (argv[i], NULL);
    GESUriClip *src = ges_uri_clip_new (uri);

    g_assert (src);
    g_free (uri);

    g_object_set (src, "duration", GST_SECOND, NULL);
    /* Since we're using a GESSimpleLayer, objects will be automatically
     * appended to the end of the layer */
    ges_layer_add_clip (layer, (GESClip *) src);
  }

  /* In order to view our timeline, let's grab a convenience pipeline to put
   * our timeline in. */
  pipeline = ges_timeline_pipeline_new ();

  /* Add the timeline to that pipeline */
  if (!ges_timeline_pipeline_add_timeline (pipeline, timeline))
    return -1;


  /* RENDER SETTINGS ! */
  /* We set our output URI and rendering setting on the pipeline */
  if (gst_uri_is_valid (argv[1])) {
    output_uri = g_strdup (argv[1]);
  } else {
    output_uri = gst_filename_to_uri (argv[1], NULL);
  }
  profile = make_encoding_profile (audio, container);
  if (!ges_timeline_pipeline_set_render_settings (pipeline, output_uri,
          profile))
    return -1;

  /* We want the pipeline to render (without any preview) */
  if (!ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_SMART_RENDER))
    return -1;


  /* The following is standard usage of a GStreamer pipeline (note how you haven't
   * had to care about GStreamer so far ?).
   *
   * We set the pipeline to playing ... */
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);

  /* ... and we start a GMainLoop. GES **REQUIRES** a GMainLoop to be running in
   * order to function properly ! */
  mainloop = g_main_loop_new (NULL, FALSE);

  /* Simple code to have the mainloop shutdown after 4s */
  /* FIXME : We should wait for EOS ! */
  g_timeout_add_seconds (argc - 1, (GSourceFunc) g_main_loop_quit, mainloop);
  g_main_loop_run (mainloop);

  return 0;
}
