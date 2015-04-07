#include "examples.h"

static void
bus_message_cb (GstBus * bus, GstMessage * message, GMainLoop * loop)
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
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GESPipeline *pipeline = ges_pipeline_new ();
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), loop);

  ges_pipeline_set_timeline (pipeline, timeline);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (loop);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
}
