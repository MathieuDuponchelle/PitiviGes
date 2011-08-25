/* GStreamer Editing Services
 * Copyright (C) 2010 Brandon Lewis <brandon@alum.berkeley.edu>
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

GESTimelineObject *make_source (gchar * path, guint64 start, guint64 duration,
    guint64 inpoint, gint priority);
void add_effect (gchar * bin_desc, GESTimelineObject * source,
    GESTrack * track);

GESTimelineObject *
make_source (gchar * path, guint64 start, guint64 duration, guint64 inpoint,
    gint priority)
{
  char *uri = g_strdup_printf ("file://%s", path);

  GESTimelineObject *ret =
      GES_TIMELINE_OBJECT (ges_timeline_filesource_new (uri));

  g_object_set (ret,
      "start", (guint64) start,
      "duration", (guint64) duration,
      "priority", (guint32) priority, "in-point", (guint64) inpoint, NULL);

  g_free (uri);

  return ret;
}

void
add_effect (gchar * bin_desc, GESTimelineObject * source, GESTrack * track)
{
  GESTrackObject *effect = NULL;
  effect = GES_TRACK_OBJECT (ges_track_parse_launch_effect_new (bin_desc));
  ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT (source), effect);
  ges_track_add_object (track, effect);
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

  GESTimeline *timeline;
  GESTrack *video_track;
  GESTimelineLayer *layer;
  GESTimelineObject *file_source;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  ctx = g_option_context_new ("- puts some effects on a media file");
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

  pipeline = ges_timeline_pipeline_new ();

  ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW_VIDEO);

  timeline = ges_timeline_new ();
  ges_timeline_pipeline_add_timeline (pipeline, timeline);

  video_track = ges_track_video_raw_new ();
  ges_timeline_add_track (timeline, video_track);

  layer = GES_TIMELINE_LAYER (ges_timeline_layer_new ());

  if (!ges_timeline_add_layer (timeline, layer))
    exit (-1);

  file_source = make_source (file_path, 0, (guint64) (duration * GST_SECOND),
      (guint64) (inpoint * GST_SECOND), 1);
  ges_timeline_layer_add_object (layer, file_source);

  add_effect ((gchar *) "videobalance hue=-1", file_source, video_track);
  add_effect ((gchar *) "agingtv", file_source, video_track);
  add_effect ((gchar *) "vertigotv", file_source, video_track);

  mainloop = g_main_loop_new (NULL, FALSE);
  g_timeout_add_seconds (duration, (GSourceFunc) g_main_loop_quit, mainloop);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (mainloop);

  return 0;
}
