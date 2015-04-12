#include "examples.h"

int
main (int args, char **argv)
{
  GESTimeline *timeline;
  GESClip *clip, *got_by_name_clip;
  GESLayer *layer;

  gst_init (NULL, NULL);
  ges_init ();

  timeline = create_timeline_with_n_layers (1);
  add_one_video_track (timeline);

  layer = ges_timeline_get_layer (timeline, 0);
  clip = GES_CLIP (ges_test_clip_new ());
  g_object_set (clip, "duration", 1 * GST_SECOND, "name",
      "my-awesome-test-clip", NULL);
  ges_layer_add_clip (layer, clip);
  gst_object_unref (layer);

  got_by_name_clip =
      GES_CLIP (ges_timeline_get_element (timeline, "my-awesome-test-clip"));
  g_object_set (got_by_name_clip, "duration", 5 * GST_SECOND, NULL);

  /* As usual, the timeline commits itself automatically when going from READY
   * to PAUSED, playback will thus last 5 seconds */
  play_timeline (timeline);
  return 0;
}
