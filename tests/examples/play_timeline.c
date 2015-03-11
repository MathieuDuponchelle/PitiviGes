#include <ges/ges.h>

static void
play_timeline (GESTimeline * timeline)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GESPipeline *pipeline = ges_pipeline_new ();

  ges_pipeline_set_timeline (pipeline, timeline);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (loop);
}
