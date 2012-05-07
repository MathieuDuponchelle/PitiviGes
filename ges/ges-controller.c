#include "ges.h"
#include "ges-controller.h"

G_DEFINE_TYPE (GESController, ges_controller, G_TYPE_OBJECT);

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
add_keyframe (source_keyframes * source_map, guint64 timestamp, GValue value)
{
  GESKeyframe *keyframe;

  keyframe = ges_keyframe_new ();
  ges_keyframe_set_timestamp (keyframe, timestamp);
  ges_keyframe_set_value (keyframe, value);


  source_map->keyframes = g_list_append (source_map->keyframes, keyframe);
  gst_interpolation_control_source_set (source_map->source, timestamp, &value);
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

static source_keyframes *
add_control_source (GESControllerPrivate * priv, const gchar * param,
    GValue value)
{
  GstInterpolationControlSource *source;
  source_keyframes *source_map;

  printf ("creating new control source for prop %s\n", param);

  source = gst_interpolation_control_source_new ();
  gst_controller_set_control_source (priv->controller, param,
      GST_CONTROL_SOURCE (source));
  if (G_VALUE_TYPE (&value) != G_TYPE_BOOLEAN)
    gst_interpolation_control_source_set_interpolation_mode (source,
        GST_INTERPOLATE_LINEAR);

  source_map = malloc (sizeof (source_keyframes));

  source_map->source = source;
  source_map->keyframes = NULL;

  g_hash_table_insert (priv->sources_table, (gchar *) param, source_map);

  return source_map;
}

GESKeyframe *
ges_controller_get_keyframe (GESController * self, const gchar * param,
    guint64 timestamp)
{
  source_keyframes *source_map;
  GESControllerPrivate *priv = self->priv;
  GList *tmp;

  printf ("we are gonna look for : %llu\n", (unsigned long long) timestamp);
  source_map = g_hash_table_lookup (priv->sources_table, param);
  if (source_map == NULL)
    return (NULL);
  for (tmp = source_map->keyframes; tmp; tmp = tmp->next)
    if (ges_keyframe_get_timestamp ((GESKeyframe *) tmp->data) == timestamp)
      return (tmp->data);
  return NULL;
}

const GList *
ges_controller_get_keyframes (GESController * self, const gchar * param)
{
  source_keyframes *source_map;
  GESControllerPrivate *priv = self->priv;

  source_map = g_hash_table_lookup (priv->sources_table, param);
  if (source_map == NULL)
    return (NULL);
  return (source_map->keyframes);
}

gboolean
ges_controller_add_keyframe (GESController * self, const gchar * param,
    guint64 timestamp, GValue value)
{
  GESControllerPrivate *priv = self->priv;
  source_keyframes *source_map;

  printf ("added keyframe\n");
  if (!priv->controller)
    if (!add_controller (priv, param))
      return (FALSE);

  source_map = g_hash_table_lookup (priv->sources_table, param);

  if (source_map == NULL)
    source_map = add_control_source (priv, param, value);

  add_keyframe (source_map, timestamp, value);

  return TRUE;
}

gboolean
ges_controller_remove_keyframe (GESController * self, const gchar * param,
    guint64 timestamp)
{
  source_keyframes *source_map;
  GESControllerPrivate *priv = self->priv;
  GList *tmp;

  printf ("we are gonna look for : %llu\n", (unsigned long long) timestamp);
  source_map = g_hash_table_lookup (priv->sources_table, param);
  if (source_map == NULL)
    return FALSE;
  for (tmp = source_map->keyframes; tmp; tmp = tmp->next)
    if (ges_keyframe_get_timestamp ((GESKeyframe *) tmp->data) == timestamp) {
      gst_interpolation_control_source_unset (source_map->source,
          ges_keyframe_get_timestamp ((GESKeyframe *) tmp->data));
      return TRUE;
    }
  return FALSE;
}

GESTrackObject *
ges_controller_get_controlled (GESController * self)
{
  return (self->priv->controlled);
}

void
ges_controller_set_controlled (GESController * self,
    GESTrackObject * controlled)
{
  self->priv->controlled = controlled;
}

GESController *
ges_controller_new (GESTrackObject * track_object)
{
  GESController *ret = NULL;

  ret = g_object_new (GES_TYPE_CONTROLLER, NULL);
  ret->priv->controlled = track_object;
  return (ret);
}
