/* GStreamer Editing Services
 * Copyright (C) 2011 Lubosz Sarnecki
 * Copyright (C) 2012 Mathieu Duponchelle <mathieu.duponchelle@epitech.eu>
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

static GESTimelineObject *
make_source (gchar * path, guint64 start, guint64 duration, guint64 inpoint,
    gint priority)
{
  char *uri = g_strdup_printf ("file://%s", path);

  GESTimelineObject *ret =
      GES_TIMELINE_OBJECT (ges_timeline_filesource_new (uri));

  g_object_set (ret, "start", (guint64) start, "duration", (guint64) duration,
      "priority", (guint32) priority, "in-point", (guint64) inpoint, NULL);

  g_free (uri);

  return ret;
}

static GESTrackObject *
create_effect (gchar * bin_desc, GESTimelineObject * source, GESTrack * track)
{
  GESTrackObject *effect = NULL;
  GESTrackParseLaunchEffect *foo = ges_track_parse_launch_effect_new (bin_desc);
  effect = GES_TRACK_OBJECT (foo);
  ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT (source), effect);
  ges_track_add_object (track, effect);
  return effect;
}

static void
init_effects (GESTimelineObject * timeline_object, GESTrack * track,
    gdouble inpoint, gdouble duration)
{
  GESTrackObject *videobalance;
  gfloat end = (gfloat) (inpoint + duration);
  GESKeyframe *keyframe;

  videobalance = create_effect (
      (gchar *) "videobalance", timeline_object, track);

  videobalance = videobalance;
  keyframe = ges_keyframe_new ();
  ges_keyframe_add_to_track_effect (keyframe, GES_TRACK_EFFECT (videobalance),
      (gchar *) "hue", GST_INTERPOLATE_LINEAR);
  ges_keyframe_set_control_point (keyframe, (gdouble) inpoint, (gdouble) - 1.0);
  ges_keyframe_set_control_point (keyframe, (gdouble) end, (gdouble) 1.0);
}

static GESTimelinePipeline *
init_pipeline (gchar * file_path, gdouble inpoint, gdouble duration)
{
  GESTimeline *timeline;
  GESTrack *video_track;
  GESTimelineLayer *layer;
  GESTimelineObject *file_source;
  GstClockTime duration_time;

  GESTimelinePipeline *pipeline;
  pipeline = ges_timeline_pipeline_new ();

  ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW_VIDEO);

  timeline = ges_timeline_new ();
  ges_timeline_pipeline_add_timeline (pipeline, timeline);

  video_track = ges_track_video_raw_new ();
  ges_timeline_add_track (timeline, video_track);

  layer = GES_TIMELINE_LAYER (ges_timeline_layer_new ());

  if (!ges_timeline_add_layer (timeline, layer))
    exit (-1);

  duration_time = (guint64) (duration * GST_SECOND);
  file_source = make_source (file_path, 0, duration_time,
      (guint64) (inpoint * GST_SECOND), 1);
  ges_timeline_layer_add_object (layer, file_source);
  init_effects (file_source, video_track, inpoint, duration);

  return pipeline;
}

int
main (int argc, char **argv)
{

  GError *err = NULL;
  GOptionContext *ctx;

  GESTimelinePipeline *pipeline;
  GMainLoop *mainloop;
  gchar *file_path;
  gdouble duration, inpoint;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  ctx =
      g_option_context_new
      ("Creates a videobalance effect, adds it to the specified source and interpolates its hue effect from -1 to 1 on the specified time interval");
  g_option_context_set_summary (ctx,
      "Select a file.\n"
      "A file is a triplet of filename, inpoint (in seconds) and duration (in seconds).\n"
      "Example:\n" "effects file1.ogv 0 5");
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing %s\n", err->message);
    exit (1);
  }

  if (argc < 4) {
    g_print ("%s", g_option_context_get_help (ctx, TRUE, NULL));
    exit (0);
  }

  g_option_context_free (ctx);

  ges_init ();

  file_path = argv[1];
  inpoint = (gdouble) atof (argv[2]);
  duration = (gdouble) atof (argv[3]);

  pipeline = init_pipeline (file_path, inpoint, duration);
  mainloop = g_main_loop_new (NULL, FALSE);

  g_timeout_add_seconds (duration, (GSourceFunc) g_main_loop_quit, mainloop);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (mainloop);
  return 0;
}
