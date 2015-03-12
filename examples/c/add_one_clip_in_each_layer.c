#include <ges/ges.h>

static void
add_one_clip_in_each_layer (GESTimeline * timeline, GstClockTime duration)
{
  GList *tmp, *layers = ges_timeline_get_layers (timeline);

  for (tmp = layers; tmp; tmp = tmp->next) {
    GESClip *clip = GES_CLIP (ges_test_clip_new ());
    GESLayer *layer = GES_LAYER (tmp->data);

    ges_layer_add_clip (layer, clip);
    g_object_set (clip, "duration", duration, "start", 0, NULL);
  }

  /* We commit the timeline to have our changes taken into account */
  ges_timeline_commit (timeline);
}
