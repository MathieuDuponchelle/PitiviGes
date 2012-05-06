/* GStreamer Editing Services
 * Copyright (C) 2010 Edward Hervey <bilboed@bilboed.com>
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

#include <stdlib.h>
#include <ges/ges.h>
#include <stdlib.h>

static void
object_added_cb (GESTrack * track, GESTrackObject * tckobj,
    GESTrackParseLaunchEffect * effect)
{
  GESController *controller;
  GValue g_value = { 0, };
  GValue g_value_bool = { 0, };
  const GESKeyframe *ret;

  if (!GES_IS_TRACK_PARSE_LAUNCH_EFFECT (tckobj)) {
    ges_track_add_object (track, GES_TRACK_OBJECT (effect));
    return;
  }

  printf ("Starting control on effect %p\n", effect);
  g_value_init (&g_value, G_TYPE_UINT);
  controller = ges_controller_new (GES_TRACK_OBJECT (effect));

  g_value_set_uint (&g_value, 0);
  ges_controller_add_keyframe (controller, "scratch-lines", 0, g_value);

  g_value_set_uint (&g_value, 20);
  ges_controller_add_keyframe (controller, "scratch-lines", 50 * GST_SECOND,
      g_value);

  g_value_init (&g_value_bool, G_TYPE_BOOLEAN);
  g_value_set_boolean (&g_value_bool, FALSE);
  ges_controller_add_keyframe (controller, "color-aging", 0, g_value_bool);
  g_value_set_boolean (&g_value_bool, TRUE);
  ges_controller_add_keyframe (controller, "color-aging", 5 * GST_SECOND,
      g_value_bool);
}

int
main (int argc, gchar ** argv)
{
  GError *err = NULL;
  GOptionContext *ctx;
  GESTimelinePipeline *pipeline;
  GESTimeline *timeline;
  GESTrack *tracka, *trackv;
  GESTimelineLayer *layer1;
  GESTimelineObject *src;
  GMainLoop *mainloop;
  GESTrackParseLaunchEffect *effect;
  gchar *uri;

  gint inpoint = 0, duration = 10;
  gboolean mute = FALSE;
  gchar *audiofile = NULL;
  GOptionEntry options[] = {
    {"inpoint", 'i', 0, G_OPTION_ARG_INT, &inpoint,
        "in-point in the file (in seconds, default:0s)", "seconds"},
    {"duration", 'd', 0, G_OPTION_ARG_INT, &duration,
        "duration to use from the file (in seconds, default:10s)", "seconds"},
    {"mute", 'm', 0, G_OPTION_ARG_NONE, &mute,
        "Whether to mute the audio from the file",},
    {"audiofile", 'a', 0, G_OPTION_ARG_FILENAME, &audiofile,
          "Use this audiofile instead of the original audio from the file",
        "audiofile"},
    {NULL}
  };

  /* Initialization and option parsing */
  if (!g_thread_supported ())
    g_thread_init (NULL);

  ctx =
      g_option_context_new
      ("- Plays an video file with sound (origin/muted/replaced)");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing %s\n", err->message);
    exit (1);
  }

  if (argc == 1) {
    g_print ("%s", g_option_context_get_help (ctx, TRUE, NULL));
    exit (0);
  }
  g_option_context_free (ctx);

  ges_init ();

  pipeline = ges_timeline_pipeline_new ();

  ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW_VIDEO);

  timeline = ges_timeline_new ();
  ges_timeline_pipeline_add_timeline (pipeline, timeline);

  tracka = ges_track_audio_raw_new ();
  ges_timeline_add_track (timeline, tracka);

  trackv = ges_track_video_raw_new ();
  ges_timeline_add_track (timeline, trackv);

  layer1 = GES_TIMELINE_LAYER (ges_timeline_layer_new ());
  g_object_set (layer1, "priority", (gint32) 0, NULL);

  ges_timeline_add_layer (timeline, layer1);

  uri = g_strdup_printf ("file://%s", argv[1]);

  /* Add the main audio/video file */

  effect = ges_track_parse_launch_effect_new ("agingtv");

  src = GES_TIMELINE_OBJECT (ges_timeline_filesource_new (uri));

  g_signal_connect (trackv, "track-object-added", G_CALLBACK (object_added_cb),
      effect);

  g_free (uri);

  g_object_set (src, "start", (guint64) 0, "in-point", (guint64) 0,
      "duration", (guint64) 200 * GST_SECOND, NULL);
  ges_timeline_layer_add_object (layer1, src);

  ges_timeline_object_add_track_object (src, GES_TRACK_OBJECT (effect));

  /* Play the pipeline */
  mainloop = g_main_loop_new (NULL, FALSE);
  g_timeout_add_seconds (200 + 1, (GSourceFunc) g_main_loop_quit, mainloop);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (mainloop);

  return 0;
}
