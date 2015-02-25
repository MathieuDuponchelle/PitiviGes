/* GStreamer Editing Services
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *               2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gestrackelement
 * @short_description: Base Class for objects contained in a GESTrack
 *
 * #GESTrackElement is the Base Class for any object that can be contained in a
 * #GESTrack.
 *
 * It contains the basic information as to the location of the object within
 * its container, like the start position, the inpoint, the duration and the
 * priority.
 */
#include "ges-internal.h"
#include "ges-extractable.h"
#include "ges-track-element.h"
#include "ges-clip.h"
#include "ges-meta-container.h"

G_DEFINE_ABSTRACT_TYPE (GESTrackElement, ges_track_element,
    GES_TYPE_TIMELINE_ELEMENT);

struct _GESTrackElementPrivate
{
  GESTrackType track_type;

  /* These fields are only used before the nleobject is available */
  guint64 pending_start;
  guint64 pending_inpoint;
  guint64 pending_duration;
  guint32 pending_priority;
  gboolean pending_active;

  GstElement *nleobject;        /* The NleObject */
  GstElement *element;          /* The element contained in the nleobject (can be NULL) */

  GESTrack *track;

  gboolean valid;
  gboolean locked;              /* If TRUE, then moves in sync with its controlling
                                 * GESClip */

  GHashTable *bindings_hashtable;       /* We need this if we want to be able to serialize
                                           and deserialize keyframes */

  GList *pending_bindings;
};

typedef struct
{
  GESTrackElement *element;
  GstControlSource *source;
  gchar *propname;
  gchar *binding_type;
} PendingBinding;

enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_TRACK_TYPE,
  PROP_TRACK,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

enum
{
  CONTROL_BINDING_ADDED,
  LAST_SIGNAL
};

static guint ges_track_element_signals[LAST_SIGNAL] = { 0 };

static GstElement *ges_track_element_create_nle_object_func (GESTrackElement *
    object);

static gboolean _set_start (GESTimelineElement * element, GstClockTime start);
static gboolean _set_inpoint (GESTimelineElement * element,
    GstClockTime inpoint);
static gboolean _set_duration (GESTimelineElement * element,
    GstClockTime duration);
static gboolean _set_priority (GESTimelineElement * element, guint32 priority);

static GParamSpec **default_list_children_properties (GESTrackElement * object,
    guint * n_properties);

static void
_update_control_bindings (GESTimelineElement * element, GstClockTime inpoint,
    GstClockTime duration);

static gboolean
_lookup_child (GESTrackElement * object,
    const gchar * prop_name, GstElement ** element, GParamSpec ** pspec)
{
  return
      GES_TIMELINE_ELEMENT_GET_CLASS (object)->lookup_child
      (GES_TIMELINE_ELEMENT (object), prop_name, (GObject **) element, pspec);
}

static gboolean
strv_find_str (const gchar ** strv, const char *str)
{
  guint i;

  if (strv == NULL)
    return FALSE;

  for (i = 0; strv[i]; i++) {
    if (g_strcmp0 (strv[i], str) == 0)
      return TRUE;
  }

  return FALSE;
}

static void
ges_track_element_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESTrackElement *track_element = GES_TRACK_ELEMENT (object);

  switch (property_id) {
    case PROP_ACTIVE:
      g_value_set_boolean (value, ges_track_element_is_active (track_element));
      break;
    case PROP_TRACK_TYPE:
      g_value_set_flags (value, track_element->priv->track_type);
      break;
    case PROP_TRACK:
      g_value_set_object (value, track_element->priv->track);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_track_element_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GESTrackElement *track_element = GES_TRACK_ELEMENT (object);

  switch (property_id) {
    case PROP_ACTIVE:
      ges_track_element_set_active (track_element, g_value_get_boolean (value));
      break;
    case PROP_TRACK_TYPE:
      track_element->priv->track_type = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_track_element_dispose (GObject * object)
{
  GESTrackElement *element = GES_TRACK_ELEMENT (object);
  GESTrackElementPrivate *priv = element->priv;

  if (priv->bindings_hashtable)
    g_hash_table_destroy (priv->bindings_hashtable);

  if (priv->nleobject) {
    GstState cstate;

    if (priv->track != NULL) {
      g_error ("%p Still in %p, this means that you forgot"
          " to remove it from the GESTrack it is contained in. You always need"
          " to remove a GESTrackElement from its track before dropping the last"
          " reference\n"
          "This problem may also be caused by a refcounting bug in"
          " the application or GES itself.", object, priv->track);
      gst_element_get_state (priv->nleobject, &cstate, NULL, 0);
      if (cstate != GST_STATE_NULL)
        gst_element_set_state (priv->nleobject, GST_STATE_NULL);
    }

    g_object_set_qdata (G_OBJECT (priv->nleobject),
        NLE_OBJECT_TRACK_ELEMENT_QUARK, NULL);
    gst_object_unref (priv->nleobject);
    priv->nleobject = NULL;
  }

  G_OBJECT_CLASS (ges_track_element_parent_class)->dispose (object);
}

static void
ges_track_element_finalize (GObject * object)
{
  G_OBJECT_CLASS (ges_track_element_parent_class)->finalize (object);
}

static void
ges_track_element_class_init (GESTrackElementClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GESTimelineElementClass *element_class = GES_TIMELINE_ELEMENT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GESTrackElementPrivate));

  object_class->get_property = ges_track_element_get_property;
  object_class->set_property = ges_track_element_set_property;
  object_class->dispose = ges_track_element_dispose;
  object_class->finalize = ges_track_element_finalize;


  /**
   * GESTrackElement:active:
   *
   * Whether the object should be taken into account in the #GESTrack output.
   * If #FALSE, then its contents will not be used in the resulting track.
   */
  properties[PROP_ACTIVE] =
      g_param_spec_boolean ("active", "Active", "Use object in output", TRUE,
      G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ACTIVE,
      properties[PROP_ACTIVE]);

  properties[PROP_TRACK_TYPE] = g_param_spec_flags ("track-type", "Track Type",
      "The track type of the object", GES_TYPE_TRACK_TYPE,
      GES_TRACK_TYPE_UNKNOWN, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_TRACK_TYPE,
      properties[PROP_TRACK_TYPE]);

  properties[PROP_TRACK] = g_param_spec_object ("track", "Track",
      "The track the object is in", GES_TYPE_TRACK, G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_TRACK,
      properties[PROP_TRACK]);

  /**
   * GESTrackElement::control-binding-added:
   * @track_element: a #GESTrackElement
   * @control_binding: the #GstControlBinding that has been added
   *
   * The control-bunding-added  is emitted each time a control binding
   * is added for a child property of @track_element
   */
  ges_track_element_signals[CONTROL_BINDING_ADDED] =
      g_signal_new ("control-binding-added", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST, 0, NULL, NULL, g_cclosure_marshal_generic,
      G_TYPE_NONE, 1, GST_TYPE_CONTROL_BINDING);

  element_class->set_start = _set_start;
  element_class->set_duration = _set_duration;
  element_class->set_inpoint = _set_inpoint;
  element_class->set_priority = _set_priority;
  element_class->deep_copy = ges_track_element_copy_properties;

  klass->create_nle_object = ges_track_element_create_nle_object_func;
  klass->list_children_properties = default_list_children_properties;
  klass->lookup_child = _lookup_child;
}

static void
ges_track_element_init (GESTrackElement * self)
{
  GESTrackElementPrivate *priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_TRACK_ELEMENT, GESTrackElementPrivate);

  /* Sane default values */
  priv->pending_start = 0;
  priv->pending_inpoint = 0;
  priv->pending_duration = GST_SECOND;
  priv->pending_priority = MIN_NLE_PRIO;
  priv->pending_active = TRUE;
  priv->bindings_hashtable = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);
}

static gfloat
interpolate_values_for_position (GstTimedValue * first_value,
    GstTimedValue * second_value, guint64 position)
{
  gfloat diff;
  GstClockTime interval;
  gfloat value_at_pos;

  diff = second_value->value - first_value->value;
  interval = second_value->timestamp - first_value->timestamp;

  if (position > first_value->timestamp)
    value_at_pos =
        first_value->value + ((float) (position -
            first_value->timestamp) / (float) interval) * diff;
  else
    value_at_pos =
        first_value->value - ((float) (first_value->timestamp -
            position) / (float) interval) * diff;

  value_at_pos = CLAMP (value_at_pos, 0.0, 1.0);

  return value_at_pos;
}

static void
_update_control_bindings (GESTimelineElement * element, GstClockTime inpoint,
    GstClockTime duration)
{
  GParamSpec **specs;
  guint n, n_specs;
  GstControlBinding *binding;
  GstTimedValueControlSource *source;
  GESTrackElement *self = GES_TRACK_ELEMENT (element);

  specs = ges_track_element_list_children_properties (self, &n_specs);

  for (n = 0; n < n_specs; ++n) {
    GList *values, *tmp;
    GstTimedValue *last, *first, *prev = NULL, *next = NULL;
    gfloat value_at_pos;

    binding = ges_track_element_get_control_binding (self, specs[n]->name);

    if (!binding)
      continue;

    g_object_get (binding, "control_source", &source, NULL);

    if (duration == 0) {
      gst_timed_value_control_source_unset_all (GST_TIMED_VALUE_CONTROL_SOURCE
          (source));
      continue;
    }

    values =
        gst_timed_value_control_source_get_all (GST_TIMED_VALUE_CONTROL_SOURCE
        (source));

    if (g_list_length (values) == 0)
      continue;

    first = values->data;

    for (tmp = values->next; tmp; tmp = tmp->next) {
      next = tmp->data;

      if (next->timestamp > inpoint)
        break;
    }

    value_at_pos = interpolate_values_for_position (first, next, inpoint);
    gst_timed_value_control_source_unset (source, first->timestamp);
    gst_timed_value_control_source_set (source, inpoint, value_at_pos);

    values =
        gst_timed_value_control_source_get_all (GST_TIMED_VALUE_CONTROL_SOURCE
        (source));

    if (duration != GST_CLOCK_TIME_NONE) {
      last = g_list_last (values)->data;

      for (tmp = g_list_last (values)->prev; tmp; tmp = tmp->prev) {
        prev = tmp->data;

        if (prev->timestamp < duration + inpoint)
          break;
      }

      value_at_pos =
          interpolate_values_for_position (prev, last, duration + inpoint);

      gst_timed_value_control_source_unset (source, last->timestamp);
      gst_timed_value_control_source_set (source, duration + inpoint,
          value_at_pos);
      values =
          gst_timed_value_control_source_get_all (GST_TIMED_VALUE_CONTROL_SOURCE
          (source));
    }

    for (tmp = values; tmp; tmp = tmp->next) {
      GstTimedValue *value = tmp->data;
      if (value->timestamp < inpoint)
        gst_timed_value_control_source_unset (source, value->timestamp);
      else if (duration != GST_CLOCK_TIME_NONE
          && value->timestamp > duration + inpoint)
        gst_timed_value_control_source_unset (source, value->timestamp);
    }
  }

  g_free (specs);
}

static gboolean
_set_start (GESTimelineElement * element, GstClockTime start)
{
  GESTrackElement *object = GES_TRACK_ELEMENT (element);

  if (object->priv->nleobject != NULL) {
    if (G_UNLIKELY (start == _START (object)))
      return FALSE;

    g_object_set (object->priv->nleobject, "start", start, NULL);
  } else
    object->priv->pending_start = start;

  return TRUE;
}

static gboolean
_set_inpoint (GESTimelineElement * element, GstClockTime inpoint)
{
  GESTrackElement *object = GES_TRACK_ELEMENT (element);

  if (object->priv->nleobject != NULL) {
    if (G_UNLIKELY (inpoint == _INPOINT (object)))

      return FALSE;

    g_object_set (object->priv->nleobject, "inpoint", inpoint, NULL);
  } else
    object->priv->pending_inpoint = inpoint;

  _update_control_bindings (element, inpoint, GST_CLOCK_TIME_NONE);

  return TRUE;
}

static gboolean
_set_duration (GESTimelineElement * element, GstClockTime duration)
{
  GESTrackElement *object = GES_TRACK_ELEMENT (element);
  GESTrackElementPrivate *priv = object->priv;

  if (GST_CLOCK_TIME_IS_VALID (_MAXDURATION (element)) &&
      duration > _INPOINT (object) + _MAXDURATION (element))
    duration = _MAXDURATION (element) - _INPOINT (object);

  if (priv->nleobject != NULL) {
    if (G_UNLIKELY (duration == _DURATION (object)))
      return FALSE;

    g_object_set (priv->nleobject, "duration", duration, NULL);
  } else
    priv->pending_duration = duration;

  _update_control_bindings (element, ges_timeline_element_get_inpoint (element),
      duration);

  return TRUE;
}

static gboolean
_set_priority (GESTimelineElement * element, guint32 priority)
{
  GESTrackElement *object = GES_TRACK_ELEMENT (element);

  if (priority < MIN_NLE_PRIO) {
    GST_INFO_OBJECT (element, "Priority (%d) < MIN_NLE_PRIO, setting it to %d",
        priority, MIN_NLE_PRIO);
    priority = MIN_NLE_PRIO;
  }

  GST_DEBUG ("object:%p, priority:%" G_GUINT32_FORMAT, object, priority);

  if (object->priv->nleobject != NULL) {
    if (G_UNLIKELY (priority == _PRIORITY (object)))
      return FALSE;

    g_object_set (object->priv->nleobject, "priority", priority, NULL);
  } else
    object->priv->pending_priority = priority;

  return TRUE;
}

/**
 * ges_track_element_set_active:
 * @object: a #GESTrackElement
 * @active: visibility
 *
 * Sets the usage of the @object. If @active is %TRUE, the object will be used for
 * playback and rendering, else it will be ignored.
 *
 * Returns: %TRUE if the property was toggled, else %FALSE
 */
gboolean
ges_track_element_set_active (GESTrackElement * object, gboolean active)
{
  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), FALSE);

  GST_DEBUG_OBJECT (object, "object:%p, active:%d", object, active);

  if (object->priv->nleobject != NULL) {
    if (G_UNLIKELY (active == object->active))
      return FALSE;

    g_object_set (object->priv->nleobject, "active", active, NULL);

    if (active != object->active) {
      object->active = active;
      if (GES_TRACK_ELEMENT_GET_CLASS (object)->active_changed)
        GES_TRACK_ELEMENT_GET_CLASS (object)->active_changed (object, active);
    }
  } else
    object->priv->pending_active = active;

  return TRUE;
}

void
ges_track_element_set_track_type (GESTrackElement * object, GESTrackType type)
{
  g_return_if_fail (GES_IS_TRACK_ELEMENT (object));

  if (object->priv->track_type != type) {
    object->priv->track_type = type;
    g_object_notify_by_pspec (G_OBJECT (object), properties[PROP_TRACK_TYPE]);
  }
}

GESTrackType
ges_track_element_get_track_type (GESTrackElement * object)
{
  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), GES_TRACK_TYPE_UNKNOWN);

  return object->priv->track_type;
}

/* default 'create_nle_object' virtual method implementation */
static GstElement *
ges_track_element_create_nle_object_func (GESTrackElement * self)
{
  GESTrackElementClass *klass = NULL;
  GstElement *child = NULL;
  GstElement *nleobject;

  klass = GES_TRACK_ELEMENT_GET_CLASS (self);

  if (G_UNLIKELY (self->priv->nleobject != NULL))
    goto already_have_nleobject;

  if (G_UNLIKELY (klass->nleobject_factorytype == NULL))
    goto no_nlefactory;

  GST_DEBUG ("Creating a supporting nleobject of type '%s'",
      klass->nleobject_factorytype);

  nleobject = gst_element_factory_make (klass->nleobject_factorytype, NULL);

  if (G_UNLIKELY (nleobject == NULL))
    goto no_nleobject;

  if (klass->create_element) {
    GST_DEBUG ("Calling subclass 'create_element' vmethod");
    child = klass->create_element (self);

    if (G_UNLIKELY (!child))
      goto child_failure;

    if (!gst_bin_add (GST_BIN (nleobject), child))
      goto add_failure;

    GST_DEBUG ("Succesfully got the element to put in the nleobject");
    self->priv->element = child;
  }

  GST_DEBUG ("done");
  return nleobject;


  /* ERROR CASES */

already_have_nleobject:
  {
    GST_ERROR ("Already controlling a NleObject %s",
        GST_ELEMENT_NAME (self->priv->nleobject));
    return NULL;
  }

no_nlefactory:
  {
    GST_ERROR ("No GESTrackElement::nleobject_factorytype implementation!");
    return NULL;
  }

no_nleobject:
  {
    GST_ERROR ("Error creating a nleobject of type '%s'",
        klass->nleobject_factorytype);
    return NULL;
  }

child_failure:
  {
    GST_ERROR ("create_element returned NULL");
    gst_object_unref (nleobject);
    return NULL;
  }

add_failure:
  {
    GST_ERROR ("Error adding the contents to the nleobject");
    gst_object_unref (child);
    gst_object_unref (nleobject);
    return NULL;
  }
}

static gboolean
ensure_nle_object (GESTrackElement * object)
{
  GESTrackElementClass *class;
  GstElement *nleobject;
  gboolean res = TRUE;

  if (object->priv->nleobject && object->priv->valid)
    return FALSE;

  /* 1. Create the NleObject */
  GST_DEBUG ("Creating NleObject");

  class = GES_TRACK_ELEMENT_GET_CLASS (object);

  if (G_UNLIKELY (class->create_nle_object == NULL)) {
    GST_ERROR ("No 'create_nle_object' implementation !");
    goto done;
  }

  GST_DEBUG ("Calling virtual method");

  /* 2. Fill in the NleObject */
  if (object->priv->nleobject == NULL) {

    /* call the create_nle_object virtual method */
    nleobject = class->create_nle_object (object);

    if (G_UNLIKELY (nleobject == NULL)) {
      GST_ERROR
          ("'create_nle_object' implementation returned TRUE but no NleObject is available");
      goto done;
    }

    GST_DEBUG_OBJECT (object, "Got a valid NleObject, now filling it in");

    object->priv->nleobject = gst_object_ref (nleobject);
    g_object_set_qdata (G_OBJECT (nleobject), NLE_OBJECT_TRACK_ELEMENT_QUARK,
        object);

    /* Set some properties on the NleObject */
    g_object_set (object->priv->nleobject,
        "duration", object->priv->pending_duration,
        "start", object->priv->pending_start,
        "inpoint", object->priv->pending_inpoint,
        "priority", object->priv->pending_priority,
        "active", object->priv->pending_active, NULL);

    /* Pendings values are not pending anymore */
    GES_TIMELINE_ELEMENT_START (object) = object->priv->pending_start;
    GES_TIMELINE_ELEMENT_INPOINT (object) = object->priv->pending_inpoint;
    GES_TIMELINE_ELEMENT_DURATION (object) = object->priv->pending_duration;
    GES_TIMELINE_ELEMENT_PRIORITY (object) = object->priv->pending_priority;
    object->active = object->priv->pending_active;


    if (object->priv->track != NULL)
      g_object_set (object->priv->nleobject,
          "caps", ges_track_get_caps (object->priv->track), NULL);

  }

done:
  object->priv->valid = res;

  GST_DEBUG ("Returning res:%d", res);

  return res;
}

/**
 * ges_track_element_add_children_props:
 * @self: The #GESTrackElement to set chidlren props on
 * @element: The GstElement to retrieve properties from
 * @wanted_categories: (array zero-terminated=1) (transfer none) (allow-none):
 * An array of categories of GstElement to
 * take into account (as defined in the factory meta "klass" field)
 * @blacklist: (array zero-terminated=1) (transfer none) (allow-none): A
 * blacklist of elements factory names to not take into account
 * @whitelist: (array zero-terminated=1) (transfer none) (allow-none): A list
 * of propery names to add as children properties
 *
 * Looks for the properties defines with the various parametters and add
 * them to the hashtable of children properties.
 *
 * To be used by subclasses only
 */
void
ges_track_element_add_children_props (GESTrackElement * self,
    GstElement * element, const gchar ** wanted_categories,
    const gchar ** blacklist, const gchar ** whitelist)
{
  GValue item = { 0, };
  GstIterator *it;
  GParamSpec **parray;
  GObjectClass *class;
  const gchar *klass;
  GstElementFactory *factory;
  gboolean done = FALSE;

  if (!GST_IS_BIN (element)) {
    guint i;
    GParamSpec *pspec;

    GObjectClass *class = G_OBJECT_GET_CLASS (element);

    for (i = 0; whitelist[i]; i++) {

      pspec = g_object_class_find_property (class, whitelist[i]);
      if (!pspec) {
        GST_WARNING ("no such property : %s in element : %s", whitelist[i],
            gst_element_get_name (element));
        continue;
      }

      if (pspec->flags & G_PARAM_WRITABLE) {
        ges_timeline_element_add_child_property (GES_TIMELINE_ELEMENT (self),
            pspec, G_OBJECT (element));
        GST_LOG_OBJECT (self,
            "added property %s to controllable properties successfully !",
            whitelist[i]);
      } else
        GST_WARNING
            ("the property %s for element %s exists but is not writable",
            whitelist[i], gst_element_get_name (element));

    }
    return;
  }

  /*  We go over child elements recursivly, and add writable properties to the
   *  hashtable */
  it = gst_bin_iterate_recurse (GST_BIN (element));
  while (!done) {
    switch (gst_iterator_next (it, &item)) {
      case GST_ITERATOR_OK:
      {
        gchar **categories;
        guint i;
        GstElement *child = g_value_get_object (&item);

        factory = gst_element_get_factory (child);
        klass = gst_element_factory_get_metadata (factory,
            GST_ELEMENT_METADATA_KLASS);

        if (strv_find_str (blacklist, GST_OBJECT_NAME (factory))) {
          GST_DEBUG_OBJECT (self, "%s blacklisted", GST_OBJECT_NAME (factory));
          continue;
        }

        GST_DEBUG ("Looking at element '%s' of klass '%s'",
            GST_ELEMENT_NAME (child), klass);

        categories = g_strsplit (klass, "/", 0);

        for (i = 0; categories[i]; i++) {
          if ((!wanted_categories ||
                  strv_find_str (wanted_categories, categories[i]))) {
            guint i, nb_specs;

            class = G_OBJECT_GET_CLASS (child);
            parray = g_object_class_list_properties (class, &nb_specs);
            for (i = 0; i < nb_specs; i++) {
              if ((parray[i]->flags & G_PARAM_WRITABLE) &&
                  (!whitelist || strv_find_str (whitelist, parray[i]->name))) {
                ges_timeline_element_add_child_property (GES_TIMELINE_ELEMENT
                    (self), parray[i], G_OBJECT (child));
              }
            }
            g_free (parray);

            GST_DEBUG
                ("%d configurable properties of '%s' added to property hashtable",
                nb_specs, GST_ELEMENT_NAME (child));
            break;
          }
        }

        g_strfreev (categories);
        g_value_reset (&item);
        break;
      }
      case GST_ITERATOR_RESYNC:
        /* FIXME, properly restart the process */
        GST_DEBUG ("iterator resync");
        gst_iterator_resync (it);
        break;

      case GST_ITERATOR_DONE:
        GST_DEBUG ("iterator done");
        done = TRUE;
        break;

      default:
        break;
    }
    g_value_unset (&item);
  }
  gst_iterator_free (it);
}

/* INTERNAL USAGE */
static void
_free_pending_binding (PendingBinding * pend)
{
  g_slice_free (PendingBinding, pend);
}

gboolean
ges_track_element_set_track (GESTrackElement * object, GESTrack * track)
{
  gboolean ret = TRUE;
  GST_DEBUG ("object:%p, track:%p", object, track);

  object->priv->track = track;

  if (object->priv->track) {
    /* If we already have a nleobject, we just set its caps properly */
    if (object->priv->nleobject) {
      g_object_set (object->priv->nleobject,
          "caps", ges_track_get_caps (object->priv->track), NULL);
    } else {
      ret = ensure_nle_object (object);

      /* if we had pending control bindings, add them and free them */
      if (ret && object->priv->pending_bindings) {
        GList *tmp;
        PendingBinding *pbinding;

        GST_INFO_OBJECT (object, "Asynchronously adding bindings");
        for (tmp = object->priv->pending_bindings; tmp; tmp = tmp->next) {
          pbinding = tmp->data;
          ges_track_element_set_control_source (pbinding->element,
              pbinding->source, pbinding->propname, pbinding->binding_type);
          g_free (pbinding->propname);
          g_free (pbinding->binding_type);
        }
        g_list_free_full (object->priv->pending_bindings,
            (GDestroyNotify) _free_pending_binding);
        object->priv->pending_bindings = NULL;
      }
    }
  }

  g_object_notify_by_pspec (G_OBJECT (object), properties[PROP_TRACK]);
  return ret;
}

/**
 * ges_track_element_get_all_control_bindings
 * @trackelement: The #TrackElement from which to get all set bindings
 *
 * Returns: (element-type gchar* GstControlBinding)(transfer none): A
 * #GHashTable containing all property_name: GstControlBinding
 */
GHashTable *
ges_track_element_get_all_control_bindings (GESTrackElement * trackelement)
{
  GESTrackElementPrivate *priv = GES_TRACK_ELEMENT (trackelement)->priv;

  return priv->bindings_hashtable;
}

guint32
_ges_track_element_get_layer_priority (GESTrackElement * element)
{
  if (_PRIORITY (element) < LAYER_HEIGHT + MIN_NLE_PRIO)
    return 0;

  return (_PRIORITY (element) - MIN_NLE_PRIO) / LAYER_HEIGHT;
}

/**
 * ges_track_element_get_track:
 * @object: a #GESTrackElement
 *
 * Get the #GESTrack to which this object belongs.
 *
 * Returns: (transfer none): The #GESTrack to which this object belongs. Can be %NULL if it
 * is not in any track
 */
GESTrack *
ges_track_element_get_track (GESTrackElement * object)
{
  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), NULL);

  return object->priv->track;
}

/**
 * ges_track_element_get_nleobject:
 * @object: a #GESTrackElement
 *
 * Get the GNonLin object this object is controlling.
 *
 * Returns: (transfer none): the GNonLin object this object is controlling.
 */
GstElement *
ges_track_element_get_nleobject (GESTrackElement * object)
{
  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), NULL);

  return object->priv->nleobject;
}

/**
 * ges_track_element_get_element:
 * @object: a #GESTrackElement
 *
 * Get the #GstElement this track element is controlling within GNonLin.
 *
 * Returns: (transfer none): the #GstElement this track element is controlling
 * within GNonLin.
 */
GstElement *
ges_track_element_get_element (GESTrackElement * object)
{
  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), NULL);

  return object->priv->element;
}

/**
 * ges_track_element_is_active:
 * @object: a #GESTrackElement
 *
 * Lets you know if @object will be used for playback and rendering,
 * or not.
 *
 * Returns: %TRUE if @object is active, %FALSE otherwize
 */
gboolean
ges_track_element_is_active (GESTrackElement * object)
{
  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), FALSE);

  if (G_UNLIKELY (object->priv->nleobject == NULL))
    return object->priv->pending_active;
  else
    return object->active;
}

/**
 * ges_track_element_lookup_child:
 * @object: object to lookup the property in
 * @prop_name: name of the property to look up. You can specify the name of the
 *     class as such: "ClassName::property-name", to guarantee that you get the
 *     proper GParamSpec in case various GstElement-s contain the same property
 *     name. If you don't do so, you will get the first element found, having
 *     this property and the and the corresponding GParamSpec.
 * @element: (out) (allow-none) (transfer full): pointer to a #GstElement that
 *     takes the real object to set property on
 * @pspec: (out) (allow-none) (transfer full): pointer to take the #GParamSpec
 *     describing the property
 *
 * Looks up which @element and @pspec would be effected by the given @name. If various
 * contained elements have this property name you will get the first one, unless you
 * specify the class name in @name.
 *
 * Returns: TRUE if @element and @pspec could be found. FALSE otherwise. In that
 * case the values for @pspec and @element are not modified. Unref @element after
 * usage.
 *
 * Deprecated: Use #ges_timeline_element_lookup_child
 */
gboolean
ges_track_element_lookup_child (GESTrackElement * object,
    const gchar * prop_name, GstElement ** element, GParamSpec ** pspec)
{
  return ges_timeline_element_lookup_child (GES_TIMELINE_ELEMENT (object),
      prop_name, ((GObject **) element), pspec);
}

/**
 * ges_track_element_set_child_property_by_pspec:
 * @object: a #GESTrackElement
 * @pspec: The #GParamSpec that specifies the property you want to set
 * @value: the value
 *
 * Sets a property of a child of @object.
 *
 * Deprecated: Use #ges_timeline_element_set_child_property_by_spec
 */
void
ges_track_element_set_child_property_by_pspec (GESTrackElement * object,
    GParamSpec * pspec, GValue * value)
{
  GstElement *element;
  g_return_if_fail (GES_IS_TRACK_ELEMENT (object));

  if (!ges_track_element_lookup_child (object, pspec->name, &element, &pspec))
    goto not_found;

  g_object_set_property (G_OBJECT (element), pspec->name, value);

  return;

not_found:
  {
    GST_ERROR ("The %s property doesn't exist", pspec->name);
    return;
  }
}

/**
 * ges_track_element_set_child_property_valist:
 * @object: The #GESTrackElement parent object
 * @first_property_name: The name of the first property to set
 * @var_args: value for the first property, followed optionally by more
 * name/return location pairs, followed by NULL
 *
 * Sets a property of a child of @object. If there are various child elements
 * that have the same property name, you can distinguish them using the following
 * syntax: 'ClasseName::property_name' as property name. If you don't, the
 * corresponding property of the first element found will be set.
 *
 * Deprecated: Use #ges_timeline_element_set_child_property_valist
 */
void
ges_track_element_set_child_property_valist (GESTrackElement * object,
    const gchar * first_property_name, va_list var_args)
{
  ges_timeline_element_set_child_property_valist (GES_TIMELINE_ELEMENT (object),
      first_property_name, var_args);
}

/**
 * ges_track_element_set_child_properties:
 * @object: The #GESTrackElement parent object
 * @first_property_name: The name of the first property to set
 * @...: value for the first property, followed optionally by more
 * name/return location pairs, followed by NULL
 *
 * Sets a property of a child of @object. If there are various child elements
 * that have the same property name, you can distinguish them using the following
 * syntax: 'ClasseName::property_name' as property name. If you don't, the
 * corresponding property of the first element found will be set.
 *
 * Deprecated: Use #ges_timeline_element_set_child_properties
 */
void
ges_track_element_set_child_properties (GESTrackElement * object,
    const gchar * first_property_name, ...)
{
  va_list var_args;

  g_return_if_fail (GES_IS_TRACK_ELEMENT (object));

  va_start (var_args, first_property_name);
  ges_track_element_set_child_property_valist (object, first_property_name,
      var_args);
  va_end (var_args);
}

/**
 * ges_track_element_get_child_property_valist:
 * @object: The #GESTrackElement parent object
 * @first_property_name: The name of the first property to get
 * @var_args: value for the first property, followed optionally by more
 * name/return location pairs, followed by NULL
 *
 * Gets a property of a child of @object. If there are various child elements
 * that have the same property name, you can distinguish them using the following
 * syntax: 'ClasseName::property_name' as property name. If you don't, the
 * corresponding property of the first element found will be set.
 *
 * Deprecated: Use #ges_timeline_element_get_child_property_valist
 */
void
ges_track_element_get_child_property_valist (GESTrackElement * object,
    const gchar * first_property_name, va_list var_args)
{
  ges_timeline_element_get_child_property_valist (GES_TIMELINE_ELEMENT (object),
      first_property_name, var_args);
}

/**
 * ges_track_element_list_children_properties:
 * @object: The #GESTrackElement to get the list of children properties from
 * @n_properties: (out): return location for the length of the returned array
 *
 * Gets an array of #GParamSpec* for all configurable properties of the
 * children of @object.
 *
 * Returns: (transfer full) (array length=n_properties): an array of #GParamSpec* which should be freed after use or
 * %NULL if something went wrong
 *
 * Deprecated: Use #ges_timeline_element_list_children_properties
 */
GParamSpec **
ges_track_element_list_children_properties (GESTrackElement * object,
    guint * n_properties)
{
  return
      ges_timeline_element_list_children_properties (GES_TIMELINE_ELEMENT
      (object), n_properties);
}

/**
 * ges_track_element_get_child_properties:
 * @object: The origin #GESTrackElement
 * @first_property_name: The name of the first property to get
 * @...: return location for the first property, followed optionally by more
 * name/return location pairs, followed by NULL
 *
 * Gets properties of a child of @object.
 *
 * Deprecated: Use #ges_timeline_element_get_child_properties
 */
void
ges_track_element_get_child_properties (GESTrackElement * object,
    const gchar * first_property_name, ...)
{
  va_list var_args;

  g_return_if_fail (GES_IS_TRACK_ELEMENT (object));

  va_start (var_args, first_property_name);
  ges_track_element_get_child_property_valist (object, first_property_name,
      var_args);
  va_end (var_args);
}

/**
 * ges_track_element_get_child_property_by_pspec:
 * @object: a #GESTrackElement
 * @pspec: The #GParamSpec that specifies the property you want to get
 * @value: (out): return location for the value
 *
 * Gets a property of a child of @object.
 *
 * Deprecated: Use #ges_timeline_element_get_child_property_by_pspec
 */
void
ges_track_element_get_child_property_by_pspec (GESTrackElement * object,
    GParamSpec * pspec, GValue * value)
{
  ges_timeline_element_get_child_property_by_pspec (GES_TIMELINE_ELEMENT
      (object), pspec, value);
}

/**
 * ges_track_element_set_child_property:
 * @object: The origin #GESTrackElement
 * @property_name: The name of the property
 * @value: the value
 *
 * Sets a property of a GstElement contained in @object.
 *
 * Note that #ges_track_element_set_child_property is really
 * intended for language bindings, #ges_track_element_set_child_properties
 * is much more convenient for C programming.
 *
 * Returns: %TRUE if the property was set, %FALSE otherwize
 *
 * Deprecated: use #ges_timeline_element_set_child_property instead
 */
gboolean
ges_track_element_set_child_property (GESTrackElement * object,
    const gchar * property_name, GValue * value)
{
  return ges_timeline_element_set_child_property (GES_TIMELINE_ELEMENT (object),
      property_name, value);
}

/**
 * ges_track_element_get_child_property:
 * @object: The origin #GESTrackElement
 * @property_name: The name of the property
 * @value: (out): return location for the property value, it will
 * be initialized if it is initialized with 0
 *
 * In general, a copy is made of the property contents and
 * the caller is responsible for freeing the memory by calling
 * g_value_unset().
 *
 * Gets a property of a GstElement contained in @object.
 *
 * Note that #ges_track_element_get_child_property is really
 * intended for language bindings, #ges_track_element_get_child_properties
 * is much more convenient for C programming.
 *
 * Returns: %TRUE if the property was found, %FALSE otherwize
 *
 * Deprecated: Use #ges_timeline_element_get_child_property
 */
gboolean
ges_track_element_get_child_property (GESTrackElement * object,
    const gchar * property_name, GValue * value)
{
  return ges_timeline_element_get_child_property (GES_TIMELINE_ELEMENT (object),
      property_name, value);
}

static GParamSpec **
default_list_children_properties (GESTrackElement * object,
    guint * n_properties)
{
  return
      GES_TIMELINE_ELEMENT_GET_CLASS (object)->list_children_properties
      (GES_TIMELINE_ELEMENT (object), n_properties);
}

void
ges_track_element_copy_properties (GESTimelineElement * element,
    GESTimelineElement * elementcopy)
{
  GParamSpec **specs;
  guint n, n_specs;
  GValue val = { 0 };
  GESTrackElement *copy = GES_TRACK_ELEMENT (elementcopy);

  ensure_nle_object (copy);
  specs =
      ges_track_element_list_children_properties (GES_TRACK_ELEMENT (element),
      &n_specs);
  for (n = 0; n < n_specs; ++n) {
    g_value_init (&val, specs[n]->value_type);
    ges_track_element_get_child_property_by_pspec (GES_TRACK_ELEMENT (element),
        specs[n], &val);
    ges_track_element_set_child_property_by_pspec (copy, specs[n], &val);
    g_value_unset (&val);
  }

  g_free (specs);
}

void
ges_track_element_split_bindings (GESTrackElement * element,
    GESTrackElement * new_element, guint64 position)
{
  GParamSpec **specs;
  guint n, n_specs;
  GstControlBinding *binding;
  GstTimedValueControlSource *source, *new_source;

  specs =
      ges_track_element_list_children_properties (GES_TRACK_ELEMENT (element),
      &n_specs);
  for (n = 0; n < n_specs; ++n) {
    GList *values, *tmp;
    GstTimedValue *last_value = NULL;
    gboolean past_position = FALSE;
    GstInterpolationMode mode;

    binding = ges_track_element_get_control_binding (element, specs[n]->name);
    if (!binding)
      continue;

    g_object_get (binding, "control_source", &source, NULL);

    /* FIXME : this should work as well with other types of control sources */
    if (!GST_IS_TIMED_VALUE_CONTROL_SOURCE (source))
      continue;

    new_source =
        GST_TIMED_VALUE_CONTROL_SOURCE (gst_interpolation_control_source_new
        ());

    g_object_get (source, "mode", &mode, NULL);
    g_object_set (new_source, "mode", mode, NULL);

    values =
        gst_timed_value_control_source_get_all (GST_TIMED_VALUE_CONTROL_SOURCE
        (source));
    for (tmp = values; tmp; tmp = tmp->next) {
      GstTimedValue *value = tmp->data;
      if (value->timestamp > position) {
        gfloat value_at_pos;

        /* FIXME We should be able to use gst_control_source_get_value so
         * all modes are handled. Right now that method only works if the value
         * we are looking for is between two actual keyframes which is not enough
         * in our case. bug #706621 */
        value_at_pos =
            interpolate_values_for_position (last_value, value, position);

        past_position = TRUE;

        gst_timed_value_control_source_set (new_source, position, value_at_pos);
        gst_timed_value_control_source_set (new_source, value->timestamp,
            value->value);
        gst_timed_value_control_source_unset (source, value->timestamp);
        gst_timed_value_control_source_set (source, position, value_at_pos);
      } else if (past_position) {
        gst_timed_value_control_source_unset (source, value->timestamp);
        gst_timed_value_control_source_set (new_source, value->timestamp,
            value->value);
      }
      last_value = value;
    }

    /* We only manage direct bindings, see TODO in set_control_source */
    ges_track_element_set_control_source (new_element,
        GST_CONTROL_SOURCE (new_source), specs[n]->name, "direct");
  }

  g_free (specs);
}

/**
 * ges_track_element_edit:
 * @object: the #GESTrackElement to edit
 * @layers: (element-type GESLayer): The layers you want the edit to
 *  happen in, %NULL means that the edition is done in all the
 *  #GESLayers contained in the current timeline.
 *      FIXME: This is not implemented yet.
 * @mode: The #GESEditMode in which the editition will happen.
 * @edge: The #GESEdge the edit should happen on.
 * @position: The position at which to edit @object (in nanosecond)
 *
 * Edit @object in the different exisiting #GESEditMode modes. In the case of
 * slide, and roll, you need to specify a #GESEdge
 *
 * Returns: %TRUE if the object as been edited properly, %FALSE if an error
 * occured
 */
gboolean
ges_track_element_edit (GESTrackElement * object,
    GList * layers, GESEditMode mode, GESEdge edge, guint64 position)
{
  GESTrack *track = ges_track_element_get_track (object);
  GESTimeline *timeline;

  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), FALSE);

  if (G_UNLIKELY (!track)) {
    GST_WARNING_OBJECT (object, "Trying to edit in %d mode but not in"
        "any Track yet.", mode);
    return FALSE;
  }

  timeline = GES_TIMELINE (ges_track_get_timeline (track));

  if (G_UNLIKELY (!timeline)) {
    GST_WARNING_OBJECT (object, "Trying to edit in %d mode but not in"
        "track %p no in any timeline yet.", mode, track);
    return FALSE;
  }

  switch (mode) {
    case GES_EDIT_MODE_NORMAL:
      timeline_move_object (timeline, object, layers, edge, position);
      break;
    case GES_EDIT_MODE_TRIM:
      timeline_trim_object (timeline, object, layers, edge, position);
      break;
    case GES_EDIT_MODE_RIPPLE:
      timeline_ripple_object (timeline, object, layers, edge, position);
      break;
    case GES_EDIT_MODE_ROLL:
      timeline_roll_object (timeline, object, layers, edge, position);
      break;
    case GES_EDIT_MODE_SLIDE:
      timeline_slide_object (timeline, object, layers, edge, position);
      break;
    default:
      GST_ERROR ("Unkown edit mode: %d", mode);
      return FALSE;
  }

  return TRUE;
}


/**
 * ges_track_element_set_control_source:
 * @object: the #GESTrackElement on which to set a control binding
 * @source: (element-type GstControlSource): the #GstControlSource to set on the binding.
 * @property_name: The name of the property to control.
 * @binding_type: The type of binding to create. Only "direct" is available for now.
 *
 * Creates a #GstControlBinding and adds it to the #GstElement concerned by the
 * property. Use the same syntax as #ges_track_element_lookup_child for
 * the property name.
 *
 * Returns: %TRUE if the binding could be created and added, %FALSE if an error
 * occured
 */
gboolean
ges_track_element_set_control_source (GESTrackElement * object,
    GstControlSource * source,
    const gchar * property_name, const gchar * binding_type)
{
  GESTrackElementPrivate *priv;
  GstElement *element;
  GParamSpec *pspec;
  GstControlBinding *binding;

  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), FALSE);
  priv = GES_TRACK_ELEMENT (object)->priv;

  if (G_UNLIKELY (!(GST_IS_CONTROL_SOURCE (source)))) {
    GST_WARNING
        ("You need to provide a non-null control source to build a new control binding");
    return FALSE;
  }

  if (!priv->track) {
    PendingBinding *pbinding;

    GST_INFO ("Adding this source to the future bindings");
    pbinding = g_slice_new0 (PendingBinding);
    pbinding->element = object;
    pbinding->source = source;
    pbinding->propname = g_strdup (property_name);
    pbinding->binding_type = g_strdup (binding_type);
    priv->pending_bindings = g_list_append (priv->pending_bindings, pbinding);
    return TRUE;
  }

  if (!ges_track_element_lookup_child (object, property_name, &element, &pspec)) {
    GST_WARNING ("You need to provide a valid and controllable property name");
    return FALSE;
  }

  /* TODO : update this according to new types of bindings */
  if (!g_strcmp0 (binding_type, "direct")) {
    /* First remove existing binding */
    binding =
        (GstControlBinding *) g_hash_table_lookup (priv->bindings_hashtable,
        property_name);
    if (binding) {
      GST_LOG ("Removing old binding %p for property %s", binding,
          property_name);
      gst_object_remove_control_binding (GST_OBJECT (element), binding);
    }
    binding =
        gst_direct_control_binding_new (GST_OBJECT (element), property_name,
        source);
    gst_object_add_control_binding (GST_OBJECT (element), binding);
    g_hash_table_insert (priv->bindings_hashtable, g_strdup (property_name),
        binding);
    g_signal_emit (object, ges_track_element_signals[CONTROL_BINDING_ADDED],
        0, binding);
    return TRUE;
  }

  GST_WARNING ("Binding type must be in [direct]");

  return FALSE;
}

/**
 * ges_track_element_get_control_binding:
 * @object: the #GESTrackElement in which to lookup the bindings.
 * @property_name: The property_name to which the binding is associated.
 *
 * Looks up the various controlled properties for that #GESTrackElement,
 * and returns the #GstControlBinding which controls @property_name.
 *
 * Returns: (transfer none): the #GstControlBinding associated with @property_name, or %NULL
 * if that property is not controlled.
 */
GstControlBinding *
ges_track_element_get_control_binding (GESTrackElement * object,
    const gchar * property_name)
{
  GESTrackElementPrivate *priv;
  GstControlBinding *binding;

  g_return_val_if_fail (GES_IS_TRACK_ELEMENT (object), NULL);

  priv = GES_TRACK_ELEMENT (object)->priv;

  binding =
      (GstControlBinding *) g_hash_table_lookup (priv->bindings_hashtable,
      property_name);
  return binding;
}
