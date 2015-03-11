#include <ges/ges.h>

static void
add_one_video_track (GESTimeline * timeline)
{
  GESTrack *track = GES_TRACK (ges_video_track_new ());

  ges_timeline_add_track (timeline, track);
}
