#include "ges.h"
#include "ges-controller.h"

G_DEFINE_TYPE (GESController, ges_controller, G_TYPE_OBJECT);

static gpointer
copy_keyframe (gpointer boxed)
{
  return (boxed);
}

static void
free_keyframe (gpointer boxed)
{
}

struct _GESControllerPrivate
{
  GESTrackObject *controlled;
  GstController *controller;
  GHashTable *sources_table;
};

static void
ges_controller_class_init (GESControllerClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class = object_class;
  g_boxed_type_register_static ("keyframe", copy_keyframe, free_keyframe);
  g_type_class_add_private (klass, sizeof (GESControllerPrivate));
}

static void
ges_controller_init (GESController * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_CONTROLLER, GESControllerPrivate);
  self->priv->controller = NULL;
  self->priv->controlled = NULL;
  self->priv->sources_table = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
add_keyframe (GstInterpolationControlSource * source, guint64 timestamp,
    GValue value)
{
  gst_interpolation_control_source_set (source, timestamp, &value);
}

static gboolean
add_controller (GESControllerPrivate * priv, const gchar * param)
{
  GstElement *element;
  GParamSpec *pspec;
  GObjectClass *class;
  GParamSpec **parray;
  guint i, nb_specs;
  GList *list = NULL;

  if (!ges_track_object_lookup_child (GES_TRACK_OBJECT (priv->controlled),
          param, &element, &pspec))
    return (FALSE);

  class = G_OBJECT_GET_CLASS (element);
  parray = g_object_class_list_properties (class, &nb_specs);
  for (i = 0; i < nb_specs; i++) {
    if (parray[i]->flags & (G_PARAM_WRITABLE) &&
        parray[i]->flags & (GST_PARAM_CONTROLLABLE)) {
      list = g_list_append (list, g_strdup (parray[i]->name));
    }
  }

  priv->controller = gst_controller_new_list (G_OBJECT (element), list);
  g_list_free_full (list, g_free);
  return (TRUE);
}

static GstInterpolationControlSource *
add_control_source (GESControllerPrivate * priv, const gchar * param,
    GValue value)
{
  GstInterpolationControlSource *source;

  printf ("creating new control source for prop %s\n", param);

  source = gst_interpolation_control_source_new ();
  gst_controller_set_control_source (priv->controller, param,
      GST_CONTROL_SOURCE (source));
  if (G_VALUE_TYPE (&value) != G_TYPE_BOOLEAN)
    gst_interpolation_control_source_set_interpolation_mode (source,
        GST_INTERPOLATE_LINEAR);
  g_hash_table_insert (priv->sources_table, (gchar *) param, source);

  return source;
}

gboolean
ges_controller_add_keyframe (GESController * self, const gchar * param,
    guint64 timestamp, GValue value)
{
  GESControllerPrivate *priv = self->priv;
  GstInterpolationControlSource *source;

  if (!priv->controller)
    if (!add_controller (priv, param))
      return (FALSE);

  source = g_hash_table_lookup (priv->sources_table, param);

  if (source == NULL)
    source = add_control_source (priv, param, value);

  add_keyframe (source, timestamp, value);

  return TRUE;
}

GESController *
ges_controller_new (GESTrackObject * track_object)
{
  GESController *ret = NULL;

  ret = g_object_new (GES_TYPE_CONTROLLER, NULL);
  ret->priv->controlled = track_object;
  return (ret);
}
