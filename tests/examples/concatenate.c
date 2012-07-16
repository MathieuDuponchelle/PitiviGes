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

#include <ges/ges.h>
#include <gst/pbutils/gstdiscoverer.h>
#include <gst/pbutils/encoding-profile.h>

GstDiscovererInfo *get_info_for_file (GstDiscoverer * disco, gchar * filename);

gboolean gotprofile = FALSE;
GESTimelinePipeline *pipeline = NULL;

static GstEncodingProfile *
make_profile_from_info (GstDiscovererInfo * info)
{
  GstEncodingContainerProfile *profile = NULL;
  GstDiscovererStreamInfo *sinfo = gst_discoverer_info_get_stream_info (info);

  /* Get the container format */
  if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
    GList *tmp, *substreams;

    profile = gst_encoding_container_profile_new ((gchar *) "concatenate", NULL,
        gst_discoverer_stream_info_get_caps (sinfo), NULL);

    substreams =
        gst_discoverer_container_info_get_streams ((GstDiscovererContainerInfo
            *) sinfo);

    /* For each on the formats add stream profiles */
    for (tmp = substreams; tmp; tmp = tmp->next) {
      GstDiscovererStreamInfo *stream = GST_DISCOVERER_STREAM_INFO (tmp->data);
      GstEncodingProfile *sprof = NULL;

      if (GST_IS_DISCOVERER_VIDEO_INFO (stream)) {
        sprof = (GstEncodingProfile *)
            gst_encoding_video_profile_new (gst_discoverer_stream_info_get_caps
            (stream), NULL, NULL, 1);
      } else if (GST_IS_DISCOVERER_AUDIO_INFO (stream)) {
        sprof = (GstEncodingProfile *)
            gst_encoding_audio_profile_new (gst_discoverer_stream_info_get_caps
            (stream), NULL, NULL, 1);
      } else {
        GST_WARNING ("Unsupported streams");
      }

      if (sprof)
        gst_encoding_container_profile_add_profile (profile, sprof);
    }
    if (substreams)
      gst_discoverer_stream_info_list_free (substreams);
  } else {
    GST_ERROR ("No container format !!!");
  }

  if (sinfo)
    gst_discoverer_stream_info_unref (sinfo);

  return GST_ENCODING_PROFILE (profile);
}

static void
bus_message_cb (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      g_print ("ERROR\n");
      g_main_loop_quit (mainloop);
      break;
    case GST_MESSAGE_EOS:
      g_print ("Done\n");
      g_main_loop_quit (mainloop);
      break;
    default:
      break;
  }
}

static void
material_loaded_cb (GESFileSourceMaterial * material, GAsyncResult * res,
    const char *render_uri)
{
  if (!gotprofile) {
    GstDiscovererInfo *info = ges_file_source_material_get_info (material);

    profile = make_profile_from_info (info);
    gotprofile = TRUE;
  }

  /* Since we're using a GESSimpleTimelineLayer, objects will be automatically
   * appended to the end of the layer */
  ges_timeline_layer_add_object (layer, (GESTimelineObject *) src);

  /* Last GESTimelineObject to be created, ready to render */
  if (render_uri) {
    /* RENDER SETTINGS ! */
    /* We set our output URI and rendering setting on the pipeline */
    ges_timeline_pipeline_set_render_settings (pipeline, render_uri, profile)

        /* And now, actually render the timeline */
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  }
}

int
main (int argc, gchar ** argv)
{
  GESTimeline *timeline;
  GESTimelineLayer *layer;
  GMainLoop *mainloop;
  GstEncodingProfile *profile = NULL;
  gchar *output_uri;
  guint i;
  GstBus *bus;

  if (argc < 3) {
    g_print ("Usage: %s <output uri> <list of files>\n", argv[0]);
    return -1;
  }

  gst_init (&argc, &argv);
  ges_init ();

  timeline = ges_timeline_new_audio_video ();

  layer = (GESTimelineLayer *) ges_simple_timeline_layer_new ();
  if (!ges_timeline_add_layer (timeline, layer))
    return -1;

  for (i = 2; i < argc; i++) {
    const gchar *uri = NULL;

    /* For the last source, we pass the render URI as a parameter */
    if (i == argc - 1)
      uri = argv[1]

          src = ges_timeline_material_filesource_new (argv[i],
          (GAsyncReadyCallback) material_loaded_cb, uri);
  }

  /* In order to view our timeline, let's grab a convenience pipeline to put
   * our timeline in. */
  pipeline = ges_timeline_pipeline_new ();

  /* Add the timeline to that pipeline */
  if (!ges_timeline_pipeline_add_timeline (pipeline, timeline))
    return -1;

  /* We want the pipeline to render (without any preview) */
  if (!ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_SMART_RENDER))
    return -1;


  mainloop = g_main_loop_new (NULL, FALSE);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), mainloop);

  g_main_loop_run (mainloop);

  return 0;
}
