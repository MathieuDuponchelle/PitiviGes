/* GStreamer Editing Services
 * Copyright (C) 2013 Thibault Saunier <thibault.saunier@collabora.com>
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
 * SECTION:ges-group
 * @short_description: Class that permits to group GESClip-s in a timeline,
 * letting the user manage it a single GESTimelineElement
 *
 * A #GESGroup is an object which controls one or more
 * #GESClips in one or more #GESLayer(s).
 *
 * To instanciate a group, you should use the ges_container_group method,
 * this will be responsible for deciding what subclass of #GESContainer
 * should be instaciated to group the various #GESTimelineElement passed
 * in parametter.
 */

#include "ges-group.h"
#include "ges.h"
#include "ges-internal.h"

#include <string.h>

#define parent_class ges_group_parent_class
G_DEFINE_TYPE (GESGroup, ges_group, GES_TYPE_CONTAINER);

#define GES_CHILDREN_INIBIT_SIGNAL_EMISSION (GES_CHILDREN_LAST + 1)

struct _GESGroupPrivate
{
  gboolean reseting_start;

  guint32 max_layer_prio;

  /* This is used while were are setting ourselve a proper timing value,
   * in this case the value should always be kept */
  gboolean setting_value;
};

enum
{
  PROP_0,
  PROP_LAST
};

/* static GParamSpec *properties[PROP_LAST]; */

/****************************************************
 *              Our listening of children           *
 ****************************************************/
static void
_update_our_values (GESGroup * group)
{
  GList *tmp;
  GESContainer *container = GES_CONTAINER (group);
  guint32 min_layer_prio = G_MAXINT32, max_layer_prio = 0;

  for (tmp = GES_CONTAINER_CHILDREN (group); tmp; tmp = tmp->next) {
    GESContainer *child = tmp->data;

    if (GES_IS_CLIP (child)) {
      GESLayer *layer = ges_clip_get_layer (GES_CLIP (child));
      gint32 prio = ges_layer_get_priority (layer);

      min_layer_prio = MIN (prio, min_layer_prio);
      max_layer_prio = MAX (prio, max_layer_prio);
    } else if (GES_IS_GROUP (child)) {
      gint32 prio = _PRIORITY (child), height = GES_CONTAINER_HEIGHT (child);

      min_layer_prio = MIN (prio, min_layer_prio);
      max_layer_prio = MAX ((prio + height), max_layer_prio);
    }
  }

  if (min_layer_prio != _PRIORITY (group)) {
    group->priv->setting_value = TRUE;
    _set_priority0 (GES_TIMELINE_ELEMENT (group), min_layer_prio);
    group->priv->setting_value = FALSE;
    for (tmp = GES_CONTAINER_CHILDREN (group); tmp; tmp = tmp->next) {
      GESTimelineElement *child = tmp->data;
      guint32 child_prio = GES_IS_CLIP (child) ?
          ges_clip_get_layer_priority (GES_CLIP (child)) : _PRIORITY (child);

      _ges_container_set_priority_offset (container,
          child, min_layer_prio - child_prio);
    }
  }

  group->priv->max_layer_prio = max_layer_prio;
  _ges_container_set_height (GES_CONTAINER (group),
      max_layer_prio - min_layer_prio + 1);
}

static void
_child_clip_changed_layer_cb (GESTimelineElement * clip,
    GParamSpec * arg G_GNUC_UNUSED, GESGroup * group)
{
  gint offset, layer_prio = ges_clip_get_layer_priority (GES_CLIP (clip));
  GESContainer *container = GES_CONTAINER (group);

  if (container->children_control_mode != GES_CHILDREN_UPDATE) {
    if (container->children_control_mode == GES_CHILDREN_INIBIT_SIGNAL_EMISSION) {
      container->children_control_mode = GES_CHILDREN_UPDATE;
      g_signal_stop_emission_by_name (clip, "notify::layer");
    }
    return;
  }

  offset = _ges_container_get_priority_offset (container, clip);

  if (layer_prio + offset < 0 ||
      (GES_TIMELINE_ELEMENT_TIMELINE (group) &&
          layer_prio + offset + GES_CONTAINER_HEIGHT (group) - 1 >
          g_list_length (GES_TIMELINE_ELEMENT_TIMELINE (group)->layers))) {
    GESLayer *old_layer =
        g_list_nth_data (GES_TIMELINE_ELEMENT_TIMELINE (group)->layers,
        _PRIORITY (group) - offset);

    GST_INFO_OBJECT (container, "Trying to move to a layer outside of"
        "the timeline layers, moving back to old layer (prio %i)",
        _PRIORITY (group) - offset);

    container->children_control_mode = GES_CHILDREN_INIBIT_SIGNAL_EMISSION;
    ges_clip_move_to_layer (GES_CLIP (clip), old_layer);
    g_signal_stop_emission_by_name (clip, "notify::layer");

    return;
  }

  container->initiated_move = clip;
  _set_priority0 (GES_TIMELINE_ELEMENT (group), layer_prio + offset);
  container->initiated_move = NULL;
}

static void
_child_group_priority_changed (GESTimelineElement * child,
    GParamSpec * arg G_GNUC_UNUSED, GESGroup * group)
{
  gint offset;
  GESContainer *container = GES_CONTAINER (group);

  if (container->children_control_mode != GES_CHILDREN_UPDATE) {
    GST_DEBUG_OBJECT (group, "Ignoring updated");
    return;
  }

  offset = _ges_container_get_priority_offset (container, child);

  if (_PRIORITY (group) + offset < 0 ||
      (GES_TIMELINE_ELEMENT_TIMELINE (group) &&
          _PRIORITY (group) + offset + GES_CONTAINER_HEIGHT (group) >
          g_list_length (GES_TIMELINE_ELEMENT_TIMELINE (group)->layers))) {

    GST_WARNING_OBJECT (container, "Trying to move to a layer outside of"
        "the timeline layers");

    return;
  }

  container->initiated_move = child;
  _set_priority0 (GES_TIMELINE_ELEMENT (group), _PRIORITY (child) + offset);
  container->initiated_move = NULL;
}

/****************************************************
 *              GESTimelineElement vmethods         *
 ****************************************************/
static gboolean
_ripple (GESTimelineElement * group, GstClockTime start)
{
  gboolean ret = TRUE;

  return ret;
}

static gboolean
_ripple_end (GESTimelineElement * group, GstClockTime end)
{
  gboolean ret = TRUE;

  return ret;
}

static gboolean
_roll_start (GESTimelineElement * group, GstClockTime start)
{
  gboolean ret = TRUE;

  return ret;
}

static gboolean
_roll_end (GESTimelineElement * group, GstClockTime end)
{
  gboolean ret = TRUE;

  return ret;
}

static gboolean
_trim (GESTimelineElement * group, GstClockTime start)
{
  GList *tmp;
  GstClockTime last_child_end = 0;
  GESContainer *container = GES_CONTAINER (group);
  GESTimeline *timeline = GES_TIMELINE_ELEMENT_TIMELINE (group);
  gboolean ret = TRUE, expending = (start < _START (group));

  if (timeline == NULL) {
    GST_DEBUG ("Not in a timeline yet");

    return FALSE;
  }

  container->children_control_mode = GES_CHILDREN_IGNORE_NOTIFIES;
  for (tmp = GES_CONTAINER_CHILDREN (group); tmp; tmp = tmp->next) {
    GESTimelineElement *child = tmp->data;

    if (expending) {
      /* If the start if bigger, we do not touch it (in case we are expending)
       */
      if (_START (child) > _START (group))
        continue;
      ret &= ges_timeline_element_trim (child, start);
    } else {
      if (start > _END (child))
        ret &= ges_timeline_element_trim (child, _END (child));
      else if (_START (child) < start && _DURATION (child))
        ret &= ges_timeline_element_trim (child, start);

    }
  }

  for (tmp = GES_CONTAINER_CHILDREN (group); tmp; tmp = tmp->next) {
    if (_DURATION (tmp->data))
      last_child_end =
          MAX (GES_TIMELINE_ELEMENT_END (tmp->data), last_child_end);
  }

  GES_GROUP (group)->priv->setting_value = TRUE;
  _set_start0 (group, start);
  _set_duration0 (group, last_child_end - start);
  GES_GROUP (group)->priv->setting_value = FALSE;
  container->children_control_mode = GES_CHILDREN_UPDATE;

  return ret;
}

static gboolean
_set_priority (GESTimelineElement * element, guint32 priority)
{
  GList *tmp, *layers;
  gint diff = priority - _PRIORITY (element);
  GESContainer *container = GES_CONTAINER (element);

  if (GES_GROUP (element)->priv->setting_value == TRUE)
    return TRUE;

  container->children_control_mode = GES_CHILDREN_IGNORE_NOTIFIES;
  layers = GES_TIMELINE_ELEMENT_TIMELINE (element) ?
      GES_TIMELINE_ELEMENT_TIMELINE (element)->layers : NULL;

  if (layers == NULL) {
    GST_WARNING_OBJECT (element, "Not any layer in the timeline, not doing"
        "anything, timeline: %" GST_PTR_FORMAT,
        GES_TIMELINE_ELEMENT_TIMELINE (element));

    return FALSE;
  } else if (priority + GES_CONTAINER_HEIGHT (container) - 1 >
      g_list_length (layers)) {
    GST_WARNING_OBJECT (container, "Trying to move to a layer outside of"
        "the timeline layers");
    return FALSE;
  }

  for (tmp = GES_CONTAINER_CHILDREN (element); tmp; tmp = tmp->next) {
    GESTimelineElement *child = tmp->data;

    if (child != container->initiated_move) {
      if (GES_IS_CLIP (child)) {
        guint32 layer_prio =
            ges_clip_get_layer_priority (GES_CLIP (child)) + diff;

        GST_DEBUG_OBJECT (child, "moving from layer: %i to %i",
            ges_clip_get_layer_priority (GES_CLIP (child)), layer_prio);
        ges_clip_move_to_layer (GES_CLIP (child),
            g_list_nth_data (layers, layer_prio));
      } else if (GES_IS_GROUP (child)) {
        GST_DEBUG_OBJECT (child, "moving from %i to %i",
            _PRIORITY (child), diff + _PRIORITY (child));
        ges_timeline_element_set_priority (child, diff + _PRIORITY (child));
      }
    }
  }
  container->children_control_mode = GES_CHILDREN_UPDATE;

  return TRUE;
}

static gboolean
_set_start (GESTimelineElement * element, GstClockTime start)
{
  GList *tmp;
  gint64 diff = start - _START (element);
  GESContainer *container = GES_CONTAINER (element);

  if (GES_GROUP (element)->priv->setting_value == TRUE)
    /* Let GESContainer update itself */
    return GES_TIMELINE_ELEMENT_CLASS (parent_class)->set_start (element,
        start);


  container->children_control_mode = GES_CHILDREN_IGNORE_NOTIFIES;
  for (tmp = GES_CONTAINER_CHILDREN (element); tmp; tmp = tmp->next) {
    GESTimelineElement *child = (GESTimelineElement *) tmp->data;

    if (child != container->initiated_move &&
        (_END (child) > _START (element) || _END (child) > start)) {
      _set_start0 (child, _START (child) + diff);
    }
  }
  container->children_control_mode = GES_CHILDREN_UPDATE;

  return TRUE;
}

static gboolean
_set_inpoint (GESTimelineElement * element, GstClockTime inpoint)
{
  return FALSE;
}

static gboolean
_set_duration (GESTimelineElement * element, GstClockTime duration)
{
  GList *tmp;
  GstClockTime last_child_end = 0, new_end;
  GESContainer *container = GES_CONTAINER (element);
  GESGroupPrivate *priv = GES_GROUP (element)->priv;

  if (priv->setting_value == TRUE)
    /* Let GESContainer update itself */
    return GES_TIMELINE_ELEMENT_CLASS (parent_class)->set_duration (element,
        duration);

  if (container->initiated_move == NULL) {
    gboolean expending = (_DURATION (element) < duration);

    new_end = _START (element) + duration;
    container->children_control_mode = GES_CHILDREN_IGNORE_NOTIFIES;
    for (tmp = GES_CONTAINER_CHILDREN (element); tmp; tmp = tmp->next) {
      GESTimelineElement *child = tmp->data;
      GstClockTime n_dur;

      if ((!expending && _END (child) > new_end) ||
          (expending && (_END (child) >= _END (element)))) {
        n_dur = MAX (0, ((gint64) (new_end - _START (child))));
        _set_duration0 (child, n_dur);
      }
    }
    container->children_control_mode = GES_CHILDREN_UPDATE;
  }

  for (tmp = GES_CONTAINER_CHILDREN (element); tmp; tmp = tmp->next) {
    if (_DURATION (tmp->data))
      last_child_end =
          MAX (GES_TIMELINE_ELEMENT_END (tmp->data), last_child_end);
  }

  priv->setting_value = TRUE;
  _set_duration0 (element, last_child_end - _START (element));
  priv->setting_value = FALSE;

  return FALSE;
}

/****************************************************
 *                                                  *
 *  GESContainer virtual methods implementation     *
 *                                                  *
 ****************************************************/

static gboolean
_add_child (GESContainer * group, GESTimelineElement * child)
{
  g_return_val_if_fail (GES_IS_CONTAINER (child), FALSE);

  return TRUE;
}

static void
_child_added (GESContainer * group, GESTimelineElement * child)
{
  GList *children, *tmp;

  GESGroupPrivate *priv = GES_GROUP (group)->priv;
  GstClockTime last_child_end = 0, first_child_start = G_MAXUINT64;

  children = GES_CONTAINER_CHILDREN (group);

  for (tmp = children; tmp; tmp = tmp->next) {
    last_child_end = MAX (GES_TIMELINE_ELEMENT_END (tmp->data), last_child_end);
    first_child_start =
        MIN (GES_TIMELINE_ELEMENT_START (tmp->data), first_child_start);
  }

  priv->setting_value = TRUE;
  if (first_child_start != GES_TIMELINE_ELEMENT_START (group)) {
    group->children_control_mode = GES_CHILDREN_IGNORE_NOTIFIES;
    _set_start0 (GES_TIMELINE_ELEMENT (group), first_child_start);
  }

  if (last_child_end != GES_TIMELINE_ELEMENT_END (group)) {
    _set_duration0 (GES_TIMELINE_ELEMENT (group),
        last_child_end - first_child_start);
  }
  priv->setting_value = FALSE;

  group->children_control_mode = GES_CHILDREN_UPDATE;
  _update_our_values (GES_GROUP (group));

  if (GES_IS_CLIP (child)) {
    g_signal_connect (child, "notify::layer",
        (GCallback) _child_clip_changed_layer_cb, group);
  } else if (GES_IS_GROUP (child), group) {
    g_signal_connect (child, "notify::priority",
        (GCallback) _child_group_priority_changed, group);
  }
}

static void
_child_removed (GESContainer * group, GESTimelineElement * child)
{
  GList *children;
  GstClockTime first_child_start;

  _ges_container_sort_children (group);

  children = GES_CONTAINER_CHILDREN (group);

  if (GES_IS_CLIP (child))
    g_signal_handlers_disconnect_by_func (child, _child_clip_changed_layer_cb,
        group);
  else if (GES_IS_GROUP (child), group)
    g_signal_handlers_disconnect_by_func (child, _child_group_priority_changed,
        group);

  if (children == NULL) {
    GST_FIXME_OBJECT (group, "Auto destroy myself?");
    return;
  }

  first_child_start = GES_TIMELINE_ELEMENT_START (children->data);
  if (first_child_start > GES_TIMELINE_ELEMENT_START (group)) {
    group->children_control_mode = GES_CHILDREN_IGNORE_NOTIFIES;
    _set_start0 (GES_TIMELINE_ELEMENT (group), first_child_start);
    group->children_control_mode = GES_CHILDREN_UPDATE;
  }
}

static GList *
_ungroup (GESContainer * group, gboolean recursive)
{
  GList *children, *tmp, *ret = NULL;

  children = ges_container_get_children (group);
  for (tmp = children; tmp; tmp = tmp->next) {
    GESTimelineElement *child = tmp->data;

    gst_object_ref (child);
    ges_container_remove (group, child);
    ret = g_list_append (ret, child);
  }
  g_list_free_full (children, gst_object_unref);

  timeline_remove_group (GES_TIMELINE_ELEMENT_TIMELINE (group),
      GES_GROUP (group));

  return ret;
}

static GESContainer *
_group (GList * containers)
{
  GList *tmp;
  GESTimeline *timeline = NULL;
  GESContainer *ret = g_object_new (GES_TYPE_GROUP, NULL);

  for (tmp = containers; tmp; tmp = tmp->next) {
    if (!timeline) {
      timeline = GES_TIMELINE_ELEMENT_TIMELINE (tmp->data);
    } else if (timeline != GES_TIMELINE_ELEMENT_TIMELINE (tmp->data)) {
      g_object_unref (ret);

      return NULL;
    }

    ges_container_add (ret, tmp->data);
  }

  timeline_add_group (timeline, GES_GROUP (ret));

  return ret;
}


/****************************************************
 *                                                  *
 *    GObject virtual methods implementation        *
 *                                                  *
 ****************************************************/
static void
ges_group_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_group_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_group_class_init (GESGroupClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GESContainerClass *container_class = GES_CONTAINER_CLASS (klass);
  GESTimelineElementClass *element_class = GES_TIMELINE_ELEMENT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GESGroupPrivate));

  object_class->get_property = ges_group_get_property;
  object_class->set_property = ges_group_set_property;

  element_class->ripple = _ripple;
  element_class->ripple_end = _ripple_end;
  element_class->roll_start = _roll_start;
  element_class->roll_end = _roll_end;
  element_class->trim = _trim;
  element_class->set_duration = _set_duration;
  element_class->set_inpoint = _set_inpoint;
  element_class->set_start = _set_start;
  element_class->set_priority = _set_priority;
  /* TODO implement the deep_copy Virtual method */

  container_class->add_child = _add_child;
  container_class->child_added = _child_added;
  container_class->child_removed = _child_removed;
  container_class->ungroup = _ungroup;
  container_class->group = _group;
  container_class->grouping_priority = 0;
}

static void
ges_group_init (GESGroup * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_GROUP, GESGroupPrivate);

  self->priv->setting_value = FALSE;
}
