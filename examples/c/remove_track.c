#include "examples.h"

int
main (int argc, char **argv)
{
  GList *tmp, *tracks;
  GESTimeline *timeline;

  gst_init (NULL, NULL);
  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  ges_timeline_append_layer (timeline);
  add_one_clip_in_each_layer (timeline, 10 * GST_SECOND);

  tracks = ges_timeline_get_tracks (timeline);

  for (tmp = tracks; tmp; tmp = tmp->next) {
    if (GES_TRACK (tmp->data)->type == GES_TRACK_TYPE_VIDEO)
      ges_timeline_remove_track (timeline, GES_TRACK (tmp->data));
  }

  g_list_free (tracks);

  ges_timeline_commit (timeline);

  play_timeline (timeline);

  return 0;
}
