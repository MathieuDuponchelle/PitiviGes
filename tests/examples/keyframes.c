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
GESTrackObject *create_effect (gchar * bin_desc, GESTimelineObject * source,
    GESTrack * track);
GstInterpolationControlSource
    * effect_property_control_source (GESTrackObject * track_object,
    gchar * propname, GstInterpolateMode mode);
GstElement *find_effect_element_in_track_object (GESTrackObject * track_object);
GESTimelinePipeline *init_pipeline (gchar * file_path, gdouble inpoint,
    gdouble duration);
void init_effects (GESTimelineObject * timeline_object, GESTrack * track,
    gdouble inpoint, gdouble duration);
void add_keyframe_to_control_source (GstInterpolationControlSource *
    control_source, gdouble time, gdouble value);
void add_keyframe_to_control_source_uint (GstInterpolationControlSource *
    control_source, gdouble time, guint value);

GESTimelineObject *
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

GESTrackObject *
create_effect (gchar * bin_desc, GESTimelineObject * source, GESTrack * track)
{
  GESTrackObject *effect = NULL;
  GESTrackParseLaunchEffect *foo = ges_track_parse_launch_effect_new (bin_desc);
  effect = GES_TRACK_OBJECT (foo);
  ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT (source), effect);
  ges_track_add_object (track, effect);
  return effect;
}

void
add_keyframe_to_control_source (GstInterpolationControlSource * control_source,
    gdouble time, gdouble value)
{
  GstClockTime timestamp = (guint64) (time * GST_SECOND);
  GValue g_value = { 0, };
  g_value_init (&g_value, G_TYPE_DOUBLE);
  g_value_set_double (&g_value, value);
  gst_interpolation_control_source_set (control_source, timestamp, &g_value);
}

void
add_keyframe_to_control_source_uint (GstInterpolationControlSource *
    control_source, gdouble time, guint value)
{
  GstClockTime timestamp = (guint64) (time * GST_SECOND);
  GValue g_value = { 0, };
  g_value_init (&g_value, G_TYPE_UINT);
  g_value_set_uint (&g_value, value);
  gst_interpolation_control_source_set (control_source, timestamp, &g_value);
}

GstInterpolationControlSource *
effect_property_control_source (GESTrackObject * track_object, gchar * propname,
    GstInterpolateMode mode)
{
  GstInterpolationControlSource *control_source;
  GstController *controller;

  GstElement *target = find_effect_element_in_track_object (track_object);

  /* add a controller to the source */
  if (!(controller = gst_object_control_properties (G_OBJECT (target),
              propname, NULL))) {
    GST_WARNING ("can't control source element");
  }
  control_source = gst_interpolation_control_source_new ();

  gst_controller_set_control_source (controller, propname,
      GST_CONTROL_SOURCE (control_source));
  gst_interpolation_control_source_set_interpolation_mode (control_source,
      mode);

  return control_source;
}

GstElement *
find_effect_element_in_track_object (GESTrackObject * track_object)
{
  GstIterator *it;
  gboolean done = FALSE;
  gpointer data;
  const gchar *klass;
  GstElementFactory *factory;
  GstElement *effect_element = NULL;
  GstBin *bin = GST_BIN (ges_track_object_get_element (track_object));

  it = gst_bin_iterate_recurse (bin);

  while (!done) {
    switch (gst_iterator_next (it, &data)) {
      case GST_ITERATOR_OK:{
        gchar **categories;
        guint category;
        GstElement *child = GST_ELEMENT_CAST (data);
        factory = gst_element_get_factory (child);
        klass = gst_element_factory_get_klass (factory);

        categories = g_strsplit (klass, "/", 0);
        for (category = 0; categories[category]; category++) {
          if (g_strcmp0 (categories[category], "Effect") == 0) {
            g_print ("Found Effect Element %s\n", GST_ELEMENT_NAME (child));
            effect_element = child;
          }
        }
        g_strfreev (categories);
        gst_object_unref (child);
        break;
      }
      case GST_ITERATOR_RESYNC:
        /* FIXME, properly restart the process */
        GST_DEBUG ("iterator resync");
        gst_iterator_resync (it);
        break;

      case GST_ITERATOR_DONE:
        GST_DEBUG ("iterator done");
        done = TRUE;
        break;

      default:
        break;
    }
  }
  gst_iterator_free (it);
  return effect_element;
}

void
init_effects (GESTimelineObject * timeline_object, GESTrack * track,
    gdouble inpoint, gdouble duration)
{
  GESTrackObject *videobalance;
  GESTrackObject *gamma;
  GstInterpolationControlSource *saturation_control;
  GstInterpolationControlSource *gamma_control;
  gfloat end = (gfloat) (inpoint + duration);
  gfloat middle = (gfloat) (inpoint + duration * 0.5);

  videobalance = create_effect (
      (gchar *) "videobalance", timeline_object, track);

  saturation_control = effect_property_control_source (videobalance,
      (gchar *) "saturation", GST_INTERPOLATE_LINEAR);

  add_keyframe_to_control_source (saturation_control, inpoint, 0);
  add_keyframe_to_control_source (saturation_control, middle, 1.5);
  add_keyframe_to_control_source (saturation_control, end, 0);

  gamma = create_effect ((gchar *) "gamma", timeline_object, track);

  gamma_control = effect_property_control_source (gamma,
      (gchar *) "gamma", GST_INTERPOLATE_QUADRATIC);

  add_keyframe_to_control_source (gamma_control, inpoint, 0);
  add_keyframe_to_control_source (gamma_control, middle, 2.0);
  add_keyframe_to_control_source (gamma_control, end, 0);
}

GESTimelinePipeline *
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

  pipeline = init_pipeline (file_path, inpoint, duration);
  mainloop = g_main_loop_new (NULL, FALSE);

  g_timeout_add_seconds (duration, (GSourceFunc) g_main_loop_quit, mainloop);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (mainloop);
  return 0;
}
