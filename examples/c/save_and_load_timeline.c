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

  if (!ges_timeline_save_to_uri (timeline, "file:///tmp/test.xges", NULL, TRUE,
          NULL))
    return 1;

  gst_object_unref (timeline);

  timeline = ges_timeline_new_from_uri ("file:///tmp/test.xges", NULL);
  if (timeline == NULL)
    return 1;

  play_timeline (timeline);
  return 0;
}
