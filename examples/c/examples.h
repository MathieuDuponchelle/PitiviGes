#ifndef __GES_EXAMPLES_H__
#define __GES_EXAMPLES_H__

#include <ges/ges.h>

void          add_one_video_track           (GESTimeline * timeline);

void          add_one_clip_in_each_layer    (GESTimeline * timeline,
                                             GstClockTime duration);

GESTimeline * create_timeline_with_n_layers (guint n_layers);

void          play_timeline                 (GESTimeline * timeline);

#endif /* __GES_EXAMPLES_H__ */ 
