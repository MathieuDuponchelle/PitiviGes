/* Gstreamer Editing Services
 *
 * Copyright (C) <2012> Thibault Saunier <thibault.saunier@collabora.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "ges.h"
#include "ges-internal.h"

#define parent_class ges_base_xml_formatter_parent_class
G_DEFINE_ABSTRACT_TYPE (GESBaseXmlFormatter, ges_base_xml_formatter,
    GES_TYPE_FORMATTER);

#define _GET_PRIV(o)\
  (((GESBaseXmlFormatter*) o)->priv)


static gboolean _loading_done_cb (GESFormatter * self);

typedef struct PendingGroup
{
  GESGroup *group;

  GList *pending_children;
} PendingGroup;

typedef struct LayerEntry
{
  GESLayer *layer;
  gboolean auto_trans;
} LayerEntry;

struct _GESBaseXmlFormatterPrivate
{
  GMarkupParseContext *parsecontext;
  gboolean check_only;

  /* Clip.ID -> Clip */
  GHashTable *containers;

  /* ID -> track */
  GHashTable *tracks;

  /* layer.prio -> LayerEntry */
  GHashTable *layers;

  /* current track element */
  GESTrackElement *current_track_element;

  GESClip *current_clip;

  gboolean timeline_auto_transition;

  GList *groups;
};

static void
_free_layer_entry (LayerEntry * entry)
{
  gst_object_unref (entry->layer);
  g_slice_free (LayerEntry, entry);
}

static GMarkupParseContext *
create_parser_context (GESBaseXmlFormatter * self, const gchar * uri,
    GError ** error)
{
  gsize xmlsize;
  GFile *file = NULL;
  gchar *xmlcontent = NULL;
  GMarkupParseContext *parsecontext = NULL;
  GESBaseXmlFormatterClass *self_class =
      GES_BASE_XML_FORMATTER_GET_CLASS (self);

  GError *err = NULL;

  if ((file = g_file_new_for_uri (uri)) == NULL)
    goto wrong_uri;

  /* TODO Handle GCancellable */
  if (!g_file_load_contents (file, NULL, &xmlcontent, &xmlsize, NULL, &err))
    goto failed;

  if (g_strcmp0 (xmlcontent, "") == 0)
    goto failed;

  parsecontext = g_markup_parse_context_new (&self_class->content_parser,
      G_MARKUP_TREAT_CDATA_AS_TEXT, self, NULL);

  if (g_markup_parse_context_parse (parsecontext, xmlcontent, xmlsize,
          &err) == FALSE)
    goto failed;

  if (!g_markup_parse_context_end_parse (parsecontext, &err))
    goto failed;

done:
  if (xmlcontent)
    g_free (xmlcontent);

  if (file)
    gst_object_unref (file);

  return parsecontext;

wrong_uri:
  GST_WARNING ("%s wrong uri", uri);

  goto done;

failed:
  g_propagate_error (error, err);

  if (parsecontext) {
    g_markup_parse_context_free (parsecontext);
    parsecontext = NULL;
  }

  goto done;
}

/***********************************************
 *                                             *
 * GESFormatter virtual methods implementation *
 *                                             *
 ***********************************************/

static gboolean
_can_load_uri (GESFormatter * dummy_formatter, const gchar * uri,
    GError ** error)
{
  GMarkupParseContext *ctx;
  GESBaseXmlFormatter *self = GES_BASE_XML_FORMATTER (dummy_formatter);

  /* we create a temporary object so we can use it as a context */
  _GET_PRIV (self)->check_only = TRUE;


  ctx = create_parser_context (self, uri, error);
  if (!ctx)
    return FALSE;

  g_markup_parse_context_free (ctx);
  return TRUE;
}

static gboolean
_load_from_uri (GESFormatter * self, GESTimeline * timeline, const gchar * uri,
    GError ** error)
{
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);

  ges_timeline_set_auto_transition (timeline, FALSE);

  priv->parsecontext =
      create_parser_context (GES_BASE_XML_FORMATTER (self), uri, error);

  if (!priv->parsecontext)
    return FALSE;

  g_idle_add ((GSourceFunc) _loading_done_cb, g_object_ref (self));

  return TRUE;
}

static gboolean
_save_to_uri (GESFormatter * formatter, GESTimeline * timeline,
    const gchar * uri, gboolean overwrite, GError ** error)
{
  GFile *file;
  gboolean ret;
  GString *str;
  GOutputStream *stream;
  GError *lerror = NULL;

  g_return_val_if_fail (formatter->project, FALSE);

  file = g_file_new_for_uri (uri);
  stream = G_OUTPUT_STREAM (g_file_create (file, G_FILE_CREATE_NONE, NULL,
          &lerror));
  if (stream == NULL) {
    if (overwrite && lerror->code == G_IO_ERROR_EXISTS) {
      g_clear_error (&lerror);
      stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE,
              G_FILE_CREATE_NONE, NULL, &lerror));
    }

    if (stream == NULL)
      goto failed_opening_file;
  }

  str = GES_BASE_XML_FORMATTER_GET_CLASS (formatter)->save (formatter,
      timeline, error);

  if (str == NULL)
    goto serialization_failed;

  ret = g_output_stream_write_all (stream, str->str, str->len, NULL,
      NULL, &lerror);
  ret = g_output_stream_close (stream, NULL, &lerror);

  if (ret == FALSE)
    GST_WARNING_OBJECT (formatter, "Could not save %s because: %s", uri,
        lerror->message);

  g_string_free (str, TRUE);
  gst_object_unref (file);
  gst_object_unref (stream);

  if (lerror)
    g_propagate_error (error, lerror);

  return ret;

serialization_failed:
  gst_object_unref (file);

  g_output_stream_close (stream, NULL, NULL);
  gst_object_unref (stream);
  if (lerror)
    g_propagate_error (error, lerror);

  return FALSE;

failed_opening_file:
  gst_object_unref (file);

  GST_WARNING_OBJECT (formatter, "Could not open %s because: %s", uri,
      lerror->message);

  if (lerror)
    g_propagate_error (error, lerror);

  return FALSE;
}

/***********************************************
 *                                             *
 *   GOBject virtual methods implementation    *
 *                                             *
 ***********************************************/

static void
_dispose (GObject * object)
{
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (object);

  g_clear_pointer (&priv->containers, (GDestroyNotify) g_hash_table_unref);
  g_clear_pointer (&priv->tracks, (GDestroyNotify) g_hash_table_unref);
  g_clear_pointer (&priv->layers, (GDestroyNotify) g_hash_table_unref);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
_finalize (GObject * object)
{
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (object);

  if (priv->parsecontext != NULL)
    g_markup_parse_context_free (priv->parsecontext);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ges_base_xml_formatter_init (GESBaseXmlFormatter * self)
{
  GESBaseXmlFormatterPrivate *priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_BASE_XML_FORMATTER, GESBaseXmlFormatterPrivate);

  priv = self->priv;

  priv->check_only = FALSE;
  priv->parsecontext = NULL;
  priv->containers = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, gst_object_unref);
  priv->tracks = g_hash_table_new_full (g_str_hash,
      g_str_equal, g_free, gst_object_unref);
  priv->layers = g_hash_table_new_full (g_direct_hash,
      g_direct_equal, NULL, (GDestroyNotify) _free_layer_entry);
  priv->current_track_element = NULL;
  priv->current_clip = NULL;
  priv->timeline_auto_transition = FALSE;
}

static void
ges_base_xml_formatter_class_init (GESBaseXmlFormatterClass * self_class)
{
  GESFormatterClass *formatter_klass = GES_FORMATTER_CLASS (self_class);
  GObjectClass *object_class = G_OBJECT_CLASS (self_class);

  g_type_class_add_private (self_class, sizeof (GESBaseXmlFormatterPrivate));
  object_class->dispose = _dispose;
  object_class->finalize = _finalize;

  formatter_klass->can_load_uri = _can_load_uri;
  formatter_klass->load_from_uri = _load_from_uri;
  formatter_klass->save_to_uri = _save_to_uri;

  self_class->save = NULL;
}

/***********************************************
 *                                             *
 *             Private methods                 *
 *                                             *
 ***********************************************/


static GESTrackElement *
_get_element_by_track_id (GESBaseXmlFormatterPrivate * priv,
    const gchar * track_id, GESClip * clip)
{
  GESTrack *track = g_hash_table_lookup (priv->tracks, track_id);

  return ges_clip_find_track_element (clip, track, GES_TYPE_SOURCE);
}

static void
_set_auto_transition (gpointer prio, LayerEntry * entry, gpointer udata)
{
  ges_layer_set_auto_transition (entry->layer, entry->auto_trans);
}

static void
_add_all_groups (GESFormatter * self)
{
  GList *tmp;
  GESTimelineElement *child;
  GESBaseXmlFormatterPrivate *priv = GES_BASE_XML_FORMATTER (self)->priv;

  for (tmp = priv->groups; tmp; tmp = tmp->next) {
    GList *lchild;
    PendingGroup *pgroup = tmp->data;

    for (lchild = ((PendingGroup *) tmp->data)->pending_children; lchild;
        lchild = lchild->next) {
      child = g_hash_table_lookup (priv->containers, lchild->data);

      GST_DEBUG_OBJECT (tmp->data, "Adding %s child %" GST_PTR_FORMAT " %s",
          (const gchar *) lchild->data, child,
          GES_TIMELINE_ELEMENT_NAME (child));
      ges_timeline_element_set_timeline (GES_TIMELINE_ELEMENT (pgroup->group),
          self->timeline);
      ges_container_add (GES_CONTAINER (pgroup->group), child);
    }
  }
}

static void
_loading_done (GESFormatter * self)
{
  GESBaseXmlFormatterPrivate *priv = GES_BASE_XML_FORMATTER (self)->priv;

  _add_all_groups (self);

  if (priv->parsecontext)
    g_markup_parse_context_free (priv->parsecontext);
  priv->parsecontext = NULL;

  ges_timeline_set_auto_transition (self->timeline,
      priv->timeline_auto_transition);

  g_hash_table_foreach (priv->layers, (GHFunc) _set_auto_transition, NULL);
  GST_ERROR ("project loaded believe it or not");
  ges_project_set_loaded (self->project, self);
}

static gboolean
_loading_done_cb (GESFormatter * self)
{
  _loading_done (self);
  g_object_unref (self);

  return FALSE;
}

static gboolean
_set_child_property (GQuark field_id, const GValue * value,
    GESTrackElement * effect)
{
  GParamSpec *pspec;
  GstElement *element;

  /* FIXME: error handling? */
  if (!ges_track_element_lookup_child (effect,
          g_quark_to_string (field_id), &element, &pspec))
    return TRUE;

  g_object_set_property (G_OBJECT (element), pspec->name, value);
  g_param_spec_unref (pspec);
  gst_object_unref (element);
  return TRUE;
}

gboolean
set_property_foreach (GQuark field_id, const GValue * value, GObject * object)
{
  g_object_set_property (object, g_quark_to_string (field_id), value);
  return TRUE;
}

static inline GESClip *
_add_object_to_layer (GESBaseXmlFormatterPrivate * priv, const gchar * id,
    GESLayer * layer, GESAsset * asset, GstClockTime start,
    GstClockTime inpoint, GstClockTime duration,
    GESTrackType track_types, const gchar * metadatas,
    GstStructure * properties)
{
  GESClip *clip;

  GST_ERROR ("adding asset to layer : %s",
      g_type_name (G_TYPE_FROM_INSTANCE (asset)));
  clip =
      ges_layer_add_asset (layer, asset, start, inpoint, duration, track_types);

  if (clip == NULL) {
    GST_WARNING_OBJECT (clip, "Could not add object from asset: %s",
        ges_asset_get_id (asset));

    return NULL;
  }

  if (metadatas)
    ges_meta_container_add_metas_from_string (GES_META_CONTAINER (clip),
        metadatas);

  if (properties)
    gst_structure_foreach (properties,
        (GstStructureForeachFunc) set_property_foreach, clip);

  g_hash_table_insert (priv->containers, g_strdup (id), gst_object_ref (clip));
  return clip;
}

static inline GESClip *
_add_clip_to_layer (GESBaseXmlFormatterPrivate * priv, const gchar * id,
    GESLayer * layer, GstClockTime start,
    GstClockTime inpoint, GstClockTime duration,
    GESTrackType track_types, const gchar * metadatas,
    GstStructure * properties)
{
  GESClip *clip;

  GST_ERROR ("adding clip from uri %s", id);
  clip = ges_layer_add_clip_from_uri (layer,
      id, start, inpoint, duration, track_types);

  if (clip == NULL) {
    GST_WARNING_OBJECT (clip, "Could not add object from uri: %s", id);

    return NULL;
  }

  if (metadatas)
    ges_meta_container_add_metas_from_string (GES_META_CONTAINER (clip),
        metadatas);

  if (properties)
    gst_structure_foreach (properties,
        (GstStructureForeachFunc) set_property_foreach, clip);

  g_hash_table_insert (priv->containers, g_strdup (id), gst_object_ref (clip));
  return clip;
}

static void
_add_track_element (GESFormatter * self, GESClip * clip,
    GESTrackElement * trackelement, const gchar * track_id,
    GstStructure * children_properties, GstStructure * properties)
{
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);
  GESTrack *track = g_hash_table_lookup (priv->tracks, track_id);

  if (track == NULL) {
    GST_WARNING_OBJECT (self, "No track with id %s, can not add trackelement",
        track_id);
    gst_object_unref (trackelement);
    return;
  }

  GST_DEBUG_OBJECT (self, "Adding track_element: %" GST_PTR_FORMAT
      " To : %" GST_PTR_FORMAT, trackelement, clip);

  ges_container_add (GES_CONTAINER (clip), GES_TIMELINE_ELEMENT (trackelement));
  gst_structure_foreach (children_properties,
      (GstStructureForeachFunc) _set_child_property, trackelement);

  if (properties)
    gst_structure_foreach (properties,
        (GstStructureForeachFunc) set_property_foreach, trackelement);
}

static GstEncodingProfile *
_create_profile (GESBaseXmlFormatter * self,
    const gchar * type, const gchar * parent, const gchar * name,
    const gchar * description, GstCaps * format, const gchar * preset,
    const gchar * preset_name, gint id, guint presence, GstCaps * restriction,
    guint pass, gboolean variableframerate)
{
  GstEncodingProfile *profile = NULL;

  if (!g_strcmp0 (type, "container")) {
    profile = GST_ENCODING_PROFILE (gst_encoding_container_profile_new (name,
            description, format, preset));
    gst_encoding_profile_set_preset_name (profile, preset_name);

    return profile;
  } else if (!g_strcmp0 (type, "video")) {
    GstEncodingVideoProfile *sprof = gst_encoding_video_profile_new (format,
        preset, restriction, presence);

    gst_encoding_video_profile_set_variableframerate (sprof, variableframerate);
    gst_encoding_video_profile_set_pass (sprof, pass);

    profile = GST_ENCODING_PROFILE (sprof);
  } else if (!g_strcmp0 (type, "audio")) {
    profile = GST_ENCODING_PROFILE (gst_encoding_audio_profile_new (format,
            preset, restriction, presence));
  } else {
    GST_ERROR_OBJECT (self, "Unknown profile format '%s'", type);

    return NULL;
  }

  gst_encoding_profile_set_name (profile, name);
  gst_encoding_profile_set_description (profile, description);
  gst_encoding_profile_set_preset_name (profile, preset_name);

  return profile;
}

/***********************************************
 *                                             *
 *              Public methods                 *
 *                                             *
 ***********************************************/

void
ges_base_xml_formatter_add_clip (GESBaseXmlFormatter * self,
    const gchar * id, const char *asset_id, GType type, GstClockTime start,
    GstClockTime inpoint, GstClockTime duration,
    guint layer_prio, GESTrackType track_types, GstStructure * properties,
    const gchar * metadatas, GError ** error)
{
  GESAsset *asset;
  LayerEntry *entry;
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);

  if (priv->check_only)
    return;

  entry = g_hash_table_lookup (priv->layers, GINT_TO_POINTER (layer_prio));
  if (entry == NULL) {
    g_set_error (error, GES_ERROR, GES_ERROR_FORMATTER_MALFORMED_INPUT_FILE,
        "We got a Clip in a layer"
        " that does not exist, something is wrong either in the project file or"
        " in %s", g_type_name (G_OBJECT_TYPE (self)));
    return;
  }

  /* We do not want the properties that are passed to layer-add_asset to be reset */
  if (properties)
    gst_structure_remove_fields (properties, "supported-formats",
        "inpoint", "start", "duration", NULL);

  asset = ges_asset_request (type, asset_id, NULL);
  if (type == GES_TYPE_URI_CLIP)
    priv->current_clip = _add_clip_to_layer (priv, asset_id, entry->layer,
        start, inpoint, duration, track_types, metadatas, properties);
  else
    priv->current_clip = _add_object_to_layer (priv, id, entry->layer,
        asset, start, inpoint, duration, track_types, metadatas, properties);
}

void
ges_base_xml_formatter_set_timeline_properties (GESBaseXmlFormatter * self,
    GESTimeline * timeline, const gchar * properties, const gchar * metadatas)
{
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);
  gboolean auto_transition = FALSE;

  if (properties) {
    GstStructure *props = gst_structure_from_string (properties, NULL);

    if (props) {
      if (gst_structure_get_boolean (props, "auto-transition",
              &auto_transition))
        gst_structure_remove_field (props, "auto-transition");

      gst_structure_foreach (props,
          (GstStructureForeachFunc) set_property_foreach, timeline);
      gst_structure_free (props);
    }
  }

  if (metadatas) {
    ges_meta_container_add_metas_from_string (GES_META_CONTAINER (timeline),
        metadatas);
  };

  priv->timeline_auto_transition = auto_transition;
}

void
ges_base_xml_formatter_add_layer (GESBaseXmlFormatter * self,
    GType extractable_type, guint priority, GstStructure * properties,
    const gchar * metadatas, GError ** error)
{
  LayerEntry *entry;
  GESAsset *asset;
  GESLayer *layer;
  gboolean auto_transition = FALSE;
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);

  if (priv->check_only)
    return;

  if (extractable_type == G_TYPE_NONE)
    layer = ges_layer_new ();
  else {
    asset = ges_asset_request (extractable_type, NULL, error);
    if (asset == NULL) {
      if (error && *error == NULL) {
        g_set_error (error, G_MARKUP_ERROR,
            G_MARKUP_ERROR_INVALID_CONTENT,
            "Layer type %s could not be created'",
            g_type_name (extractable_type));
        return;
      }
    }
    layer = GES_LAYER (ges_asset_extract (asset, error));
  }

  ges_layer_set_priority (layer, priority);
  ges_timeline_add_layer (GES_FORMATTER (self)->timeline, layer);
  if (properties) {
    if (gst_structure_get_boolean (properties, "auto-transition",
            &auto_transition))
      gst_structure_remove_field (properties, "auto-transition");

    gst_structure_foreach (properties,
        (GstStructureForeachFunc) set_property_foreach, layer);
  }

  if (metadatas)
    ges_meta_container_add_metas_from_string (GES_META_CONTAINER (layer),
        metadatas);

  entry = g_slice_new0 (LayerEntry);
  entry->layer = gst_object_ref (layer);
  entry->auto_trans = auto_transition;

  g_hash_table_insert (priv->layers, GINT_TO_POINTER (priority), entry);
}

void
ges_base_xml_formatter_add_track (GESBaseXmlFormatter * self,
    GESTrackType track_type, GstCaps * caps, const gchar * id,
    GstStructure * properties, const gchar * metadatas, GError ** error)
{
  GESTrack *track;
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);

  if (priv->check_only) {
    if (caps)
      gst_caps_unref (caps);

    return;
  }

  track = ges_track_new (track_type, caps);
  ges_timeline_add_track (GES_FORMATTER (self)->timeline, track);

  if (properties) {
    gchar *restriction;
    GstCaps *caps;

    gst_structure_get (properties, "restriction-caps", G_TYPE_STRING,
        &restriction, NULL);
    gst_structure_remove_fields (properties, "restriction-caps", "caps",
        "message-forward", NULL);
    if (g_strcmp0 (restriction, "NULL")) {
      caps = gst_caps_from_string (restriction);
      ges_track_set_restriction_caps (track, caps);
    }
    gst_structure_foreach (properties,
        (GstStructureForeachFunc) set_property_foreach, track);
  }

  g_hash_table_insert (priv->tracks, g_strdup (id), gst_object_ref (track));
  if (metadatas)
    ges_meta_container_add_metas_from_string (GES_META_CONTAINER (track),
        metadatas);
}

void
ges_base_xml_formatter_add_control_binding (GESBaseXmlFormatter * self,
    const gchar * binding_type, const gchar * source_type,
    const gchar * property_name, gint mode, const gchar * track_id,
    GSList * timed_values)
{
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);
  GESTrackElement *element = NULL;

  if (track_id[0] != '-' && priv->current_clip)
    element = _get_element_by_track_id (priv, track_id, priv->current_clip);
  else
    element = priv->current_track_element;

  if (element == NULL) {
    GST_WARNING ("No current track element to which we can append a binding");
    return;
  }

  if (!g_strcmp0 (source_type, "interpolation")) {
    GstControlSource *source;

    source = gst_interpolation_control_source_new ();
    ges_track_element_set_control_source (element, source,
        property_name, binding_type);

    g_object_set (source, "mode", mode, NULL);

    gst_timed_value_control_source_set_from_list (GST_TIMED_VALUE_CONTROL_SOURCE
        (source), timed_values);
  } else
    GST_WARNING ("This interpolation type is not supported\n");
}

void
ges_base_xml_formatter_add_source (GESBaseXmlFormatter * self,
    const gchar * track_id, GstStructure * children_properties)
{
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);
  GESTrackElement *element = NULL;

  if (track_id[0] != '-' && priv->current_clip)
    element = _get_element_by_track_id (priv, track_id, priv->current_clip);
  else
    element = priv->current_track_element;

  if (element == NULL) {
    GST_WARNING
        ("No current track element to which we can append children properties");
    return;
  }

  gst_structure_foreach (children_properties,
      (GstStructureForeachFunc) _set_child_property, element);
}

void
ges_base_xml_formatter_add_track_element (GESBaseXmlFormatter * self,
    GType track_element_type, const gchar * asset_id, const gchar * track_id,
    const gchar * timeline_obj_id, GstStructure * children_properties,
    GstStructure * properties, const gchar * metadatas, GError ** error)
{
  GESTrackElement *trackelement;

  GError *err = NULL;
  GESAsset *asset = NULL;
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);

  if (priv->check_only)
    return;

  if (g_type_is_a (track_element_type, GES_TYPE_TRACK_ELEMENT) == FALSE) {
    GST_DEBUG_OBJECT (self, "%s is not a TrackElement, can not create it",
        g_type_name (track_element_type));
    goto out;
  }

  if (g_type_is_a (track_element_type, GES_TYPE_BASE_EFFECT) == FALSE) {
    GST_FIXME_OBJECT (self, "%s currently not supported",
        g_type_name (track_element_type));
    goto out;
  }

  asset = ges_asset_request (track_element_type, asset_id, &err);
  if (asset == NULL) {
    GST_DEBUG_OBJECT (self, "Can not create trackelement %s", asset_id);
    GST_FIXME_OBJECT (self, "Check if missing plugins etc %s",
        err ? err->message : "");

    goto out;
  }

  trackelement = GES_TRACK_ELEMENT (ges_asset_extract (asset, NULL));
  if (trackelement) {
    GESClip *clip;
    if (metadatas)
      ges_meta_container_add_metas_from_string (GES_META_CONTAINER
          (trackelement), metadatas);

    clip = g_hash_table_lookup (priv->containers, timeline_obj_id);
    if (clip) {
      _add_track_element (GES_FORMATTER (self), clip, trackelement, track_id,
          children_properties, properties);
    } else {
      GST_WARNING ("no clip to add the track element on");
    }
    priv->current_track_element = trackelement;
  }

  ges_project_add_asset (GES_FORMATTER (self)->project, asset);

out:
  if (asset)
    gst_object_unref (asset);
  if (err)
    g_error_free (err);

  return;
}

void
ges_base_xml_formatter_add_encoding_profile (GESBaseXmlFormatter * self,
    const gchar * type, const gchar * parent, const gchar * name,
    const gchar * description, GstCaps * format, const gchar * preset,
    const gchar * preset_name, guint id, guint presence, GstCaps * restriction,
    guint pass, gboolean variableframerate, GstStructure * properties,
    GError ** error)
{
  const GList *tmp;
  GstEncodingProfile *profile;
  GstEncodingContainerProfile *parent_profile = NULL;
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);

  if (priv->check_only)
    goto done;

  if (parent == NULL) {
    profile =
        _create_profile (self, type, parent, name, description, format, preset,
        preset_name, id, presence, restriction, pass, variableframerate);
    ges_project_add_encoding_profile (GES_FORMATTER (self)->project, profile);
    gst_object_unref (profile);

    goto done;
  }

  for (tmp = ges_project_list_encoding_profiles (GES_FORMATTER (self)->project);
      tmp; tmp = tmp->next) {
    GstEncodingProfile *tmpprofile = GST_ENCODING_PROFILE (tmp->data);

    if (g_strcmp0 (gst_encoding_profile_get_name (tmpprofile),
            gst_encoding_profile_get_name (tmpprofile)) == 0) {

      if (!GST_IS_ENCODING_CONTAINER_PROFILE (tmpprofile)) {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
            "Profile '%s' parent %s is not a container...'", name, parent);
        goto done;
      }

      parent_profile = GST_ENCODING_CONTAINER_PROFILE (tmpprofile);
      break;
    }
  }

  if (parent_profile == NULL) {
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
        "Profile '%s' parent %s does not exist'", name, parent);
    goto done;
  }

  profile =
      _create_profile (self, type, parent, name, description, format, preset,
      preset_name, id, presence, restriction, pass, variableframerate);

  if (profile == NULL)
    goto done;

  gst_encoding_container_profile_add_profile (parent_profile, profile);

done:
  if (format)
    gst_caps_unref (format);
  if (restriction)
    gst_caps_unref (restriction);
}

void
ges_base_xml_formatter_add_group (GESBaseXmlFormatter * self,
    const gchar * id, const gchar * properties)
{
  PendingGroup *pgroup;
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);

  if (priv->check_only)
    return;

  pgroup = g_slice_new0 (PendingGroup);
  pgroup->group = ges_group_new ();

  g_hash_table_insert (priv->containers, g_strdup (id),
      gst_object_ref (pgroup->group));
  priv->groups = g_list_prepend (priv->groups, pgroup);

  return;
}

void
ges_base_xml_formatter_last_group_add_child (GESBaseXmlFormatter * self,
    const gchar * child_id, const gchar * name)
{
  PendingGroup *pgroup;
  GESBaseXmlFormatterPrivate *priv = _GET_PRIV (self);

  if (priv->check_only)
    return;

  g_return_if_fail (priv->groups);

  pgroup = priv->groups->data;

  pgroup->pending_children =
      g_list_prepend (pgroup->pending_children, g_strdup (child_id));

  GST_DEBUG_OBJECT (self, "Adding %s to %s", child_id,
      GES_TIMELINE_ELEMENT_NAME (((PendingGroup *) priv->groups->data)->group));
}
