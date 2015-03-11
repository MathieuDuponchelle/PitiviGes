#include <ges/ges.h>
#include "add_one_clip_in_each_layer.c"
#include "add_one_video_track.c"
#include "create_timeline_with_n_layers.c"
#include "play_timeline.c"

int
main (int ac, char **av)
{
  GESTimeline *timeline;

  gst_init (NULL, NULL);
  ges_init ();

  timeline = create_timeline_with_n_layers (2);
  add_one_clip_in_each_layer (timeline, 10 * GST_SECOND);
  add_one_video_track (timeline);
  play_timeline (timeline);
  return 0;
}
