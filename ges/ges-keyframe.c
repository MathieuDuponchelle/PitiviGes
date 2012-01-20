#include "ges.h"
#include "ges-keyframe.h"

G_DEFINE_TYPE (GESKeyframe, ges_keyframe, G_TYPE_OBJECT);

struct _GESKeyframePrivate
{
  void *data;
};

static void
ges_keyframe_class_init (GESKeyframeClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class = object_class;
  g_type_class_add_private (klass, sizeof (GESKeyframePrivate));

}

static void
ges_keyframe_init (GESKeyframe * object)
{
  object = object;
}

GESKeyframe *
ges_keyframe_new (void)
{
  return g_object_new (GES_TYPE_KEYFILE_FORMATTER, NULL);
}
