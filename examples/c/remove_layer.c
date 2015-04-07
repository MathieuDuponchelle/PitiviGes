#include "examples.h"

int
main (int argc, char **argv)
{
  GList *tmp, *layers;
  GESTimeline *timeline;

  gst_init (NULL, NULL);
  ges_init ();

  timeline = create_timeline_with_n_layers (2);
  add_one_clip_in_each_layer (timeline, 10 * GST_SECOND);
  add_one_video_track (timeline);

  layers = ges_timeline_get_layers (timeline);

  for (tmp = layers; tmp; tmp = tmp->next)
    ges_timeline_remove_layer (timeline, GES_LAYER (tmp->data));

  g_list_free (layers);

  ges_timeline_commit (timeline);

  /* This will return immediately, as the timeline is now empty */
  play_timeline (timeline);

  return 0;
}
