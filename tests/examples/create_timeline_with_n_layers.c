#include <ges/ges.h>

static GESTimeline *
create_timeline_with_n_layers (guint n_layers)
{
  GESTimeline *timeline = ges_timeline_new ();
  guint i;

  for (i = n_layers; i > 0; i -= 1)
    ges_timeline_append_layer (timeline);
  return timeline;
}
