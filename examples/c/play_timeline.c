#include "examples.h"

static GMainLoop *loop;

static void
bus_message_cb (GstBus * bus, GstMessage * message)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }
}

void
play_timeline (GESTimeline * timeline)
{
  GESPipeline *pipeline = ges_pipeline_new ();
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  loop = g_main_loop_new (NULL, FALSE);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), NULL);

  ges_pipeline_set_timeline (pipeline, timeline);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (loop);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
}

void
play_pipeline (GESPipeline * pipeline)
{
  gst_element_seek_simple (GST_ELEMENT (pipeline), GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE, 0);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (loop);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PAUSED);
  gst_element_get_state (GST_ELEMENT (pipeline), NULL, NULL,
      GST_CLOCK_TIME_NONE);
}

GESPipeline *
prepare_pipeline (GESTimeline * timeline)
{
  GESPipeline *pipeline = ges_pipeline_new ();
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  loop = g_main_loop_new (NULL, FALSE);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), NULL);

  ges_pipeline_set_timeline (pipeline, timeline);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PAUSED);
  gst_element_get_state (GST_ELEMENT (pipeline), NULL, NULL,
      GST_CLOCK_TIME_NONE);

  return pipeline;
}
