#include "ges.h"
#include "ges-keyframe.h"

G_DEFINE_TYPE (GESKeyframe, ges_keyframe, G_TYPE_OBJECT);

struct _GESKeyframePrivate
{
  GstClockTime timestamp;
  GValue value;
};

static void
ges_keyframe_class_init (GESKeyframeClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class = object_class;
  g_type_class_add_private (klass, sizeof (GESKeyframePrivate));
}

static void
ges_keyframe_init (GESKeyframe * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_KEYFRAME, GESKeyframePrivate);
}

void
ges_keyframe_set_timestamp (GESKeyframe * self, GstClockTime timestamp)
{
  self->priv->timestamp = timestamp;
}

GstClockTime
ges_keyframe_get_timestamp (GESKeyframe * self)
{
  return self->priv->timestamp;
}

void
ges_keyframe_set_value (GESKeyframe * self, GValue value)
{
  memset (&(self->priv->value), 0, sizeof (self->priv->value));

  g_value_init (&(self->priv->value), G_VALUE_TYPE (&value));

  g_value_copy (&value, &(self->priv->value));
}

const GValue *
ges_keyframe_get_value (GESKeyframe * self)
{
  return &(self->priv->value);
}

GESKeyframe *
ges_keyframe_new (void)
{
  return g_object_new (GES_TYPE_KEYFRAME, NULL);
}
