#include "ges.h"
#include "ges-keyframe.h"

G_DEFINE_TYPE (GESKeyframe, ges_keyframe, G_TYPE_OBJECT);

struct _GESKeyframePrivate
{
  GstInterpolationControlSource *control_source;
  GParamSpec *pspec;
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
  self->priv->control_source = NULL;
  self->priv->pspec = NULL;
}

GESKeyframe *
ges_keyframe_new (void)
{
  return g_object_new (GES_TYPE_KEYFRAME, NULL);
}

static GstInterpolationControlSource *
effect_property_control_source (GESTrackEffect * target, gchar * propname,
    GstInterpolateMode mode, GESKeyframe * kf)
{
  GstInterpolationControlSource *control_source;
  GstController *controller;
  GstElement *element;
  GParamSpec *pspec;
  GESKeyframePrivate *priv;

  /* add a controller to the source */
  if (!ges_track_object_lookup_child (GES_TRACK_OBJECT (target), propname,
          &element, &pspec))
    return (NULL);

  if (!(controller = gst_object_control_properties (G_OBJECT (element),
              propname, NULL))) {
    return (NULL);
  }

  control_source = gst_interpolation_control_source_new ();

  gst_controller_set_control_source (controller, propname,
      GST_CONTROL_SOURCE (control_source));
  gst_interpolation_control_source_set_interpolation_mode (control_source,
      mode);
  priv = GES_KEYFRAME (kf)->priv;
  priv->pspec = pspec;
  return control_source;
}

void
ges_keyframe_set_control_point (GESKeyframe * kf, gdouble time, gdouble value)
{
  GESKeyframePrivate *priv;
  GstClockTime timestamp = (guint64) (time * GST_SECOND);
  GValue g_value = { 0, };
  GParamSpec *pspec;

  priv = GES_KEYFRAME (kf)->priv;
  pspec = priv->pspec;
  g_value_init (&g_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (G_IS_PARAM_SPEC_DOUBLE (pspec))
    g_value_set_double (&g_value, value);
  else if (G_IS_PARAM_SPEC_UINT (pspec))
    g_value_set_uint (&g_value, value);
  gst_interpolation_control_source_set (priv->control_source, timestamp,
      &g_value);
}

gint
ges_keyframe_add_to_track_effect (GESKeyframe * kf, GESTrackEffect * effect,
    gchar * propname, GstInterpolateMode mode)
{
  GstInterpolationControlSource *control_source;
  GESKeyframePrivate *priv;

  control_source = effect_property_control_source (effect, propname, mode, kf);
  priv = GES_KEYFRAME (kf)->priv;
  priv->control_source = control_source;
  if (control_source == NULL)
    return (0);
  return (1);
}
