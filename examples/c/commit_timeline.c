#include "examples.h"

int
main (int argc, char **argv)
{
  GESTimeline *timeline;
  GESPipeline *pipeline;
  GESLayer *layer;
  GESClip *clip;

  gst_init (NULL, NULL);
  ges_init ();

  /* Let's setup a timeline with a 1-second clip, this part should now look
   * pretty familiar */
  timeline = create_timeline_with_n_layers (1);
  add_one_video_track (timeline);
  layer = ges_timeline_get_layer (timeline, 0);
  clip = GES_CLIP (ges_test_clip_new ());
  g_object_set (clip, "duration", 1 * GST_SECOND, "start", 0, NULL);
  ges_layer_add_clip (layer, clip);
  gst_object_unref (layer);

  /* This returns us a paused pipeline, all the changes are commited
   * automatically when going from READY to PAUSED.
   */
  pipeline = prepare_pipeline (timeline);

  /* These changes will only be executed once we have commited the timeline */
  g_object_set (clip, "duration", 5 * GST_SECOND, NULL);

  /* Even though the duration of the clip has been set to 5 seconds, playing
   * the pipeline will only last for 1 second as we haven't commited our changes
   */
  play_pipeline (pipeline);
  g_print ("We played the pipeline once\n");
  /* Let's wait for one second just to make sure we notice the difference */
  g_usleep (1000000);

  ges_timeline_commit_sync (timeline);

  /* The changes were taken into account, this will play for 5 seconds */
  g_print ("Playing again\n");
  play_pipeline (pipeline);
  g_print ("We played the pipeline twice, bye o/\n");
  return 0;
}
