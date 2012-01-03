#include <ges/ges.h>
#include <inttypes.h>
#include <unistd.h>
#define GetCurrentDir getcwd

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

GST_DEBUG_CATEGORY_STATIC (ges_pitivi_formatter_debug);
#define GST_CAT_DEFAULT ges_pitivi_formatter_debug

/* The PiTiVi etree formatter is 0.1 we set GES one to 0.2 */
#define VERSION "0.2"

static gboolean save_pitivi_timeline_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri);
static gboolean load_pitivi_file_from_uri (GESFormatter * self,
    GESTimeline * timeline, const gchar * uri);
static void ges_pitivi_formatter_finalize (GObject * object);
gboolean add_pitivi_source (GESFormatter * formatter, const gchar * source,
    const gchar * uri);

typedef struct SrcMapping
{
  gchar *id;
  GESTimelineObject *obj;
  guint priority;
  GList *tck_obj_ids;
} SrcMapping;

struct _GESPitiviFormatterPrivate
{
  xmlXPathContextPtr xpathCtx;

  /* {"sourceId" : {"prop": "value"}} */
  GHashTable *source_table;

  /* {trackId: {"factory_ref": factoryId, ""}
   * if effect:
   *      {"factory_ref": "effect",
   *       "effect_name": name
   *       "effect_props": {"propname": value}}}
   */
  GHashTable *track_objects_table;

  /* {factory-ref: [track-object-ref-id,...]} */
  GHashTable *timeline_objects_table;

  /* {layerPriority: layer} */
  GHashTable *layers_table;

  GESTimeline *timeline;

  GESTrack *tracka, *trackv;
};

/* Memory freeing functions */
static void
free_src_map (SrcMapping * srcmap)
{
  g_free (srcmap->id);
  g_object_unref (srcmap->obj);
  g_list_free_full (srcmap->tck_obj_ids, (GDestroyNotify) g_free);
  g_slice_free (SrcMapping, srcmap);
}

static void
list_table_destroyer (gpointer key, gpointer value, void *unused)
{
  g_list_free_full (value, g_free);
}

/* Object functions */
static void
ges_pitivi_formatter_class_init (GESPitiviFormatterClass * klass)
{
  GESFormatterClass *formatter_klass;
  GObjectClass *object_class;
  object_class = G_OBJECT_CLASS (klass);
  formatter_klass = GES_FORMATTER_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GESPitiviFormatterPrivate));

  formatter_klass->save_to_uri = save_pitivi_timeline_to_uri;
  formatter_klass->load_from_uri = load_pitivi_file_from_uri;
  formatter_klass->add_source = add_pitivi_source;
  object_class->finalize = ges_pitivi_formatter_finalize;
}

static void
ges_pitivi_formatter_init (GESPitiviFormatter * self)
{
  GESPitiviFormatterPrivate *priv;

  GST_DEBUG_CATEGORY_INIT (ges_pitivi_formatter_debug, "ges_pitivi_formatter",
      GST_DEBUG_FG_YELLOW, "ges pitivi formatter");

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_PITIVI_FORMATTER, GESPitiviFormatterPrivate);

  priv = self->priv;

  priv->track_objects_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) g_hash_table_destroy);

  priv->timeline_objects_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  priv->layers_table =
      g_hash_table_new_full (g_int_hash, g_str_equal, g_free, g_object_unref);

  priv->source_table = NULL;
}

static void
ges_pitivi_formatter_finalize (GObject * object)
{
  GESPitiviFormatter *self = GES_PITIVI_FORMATTER (object);
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;

  if (priv->source_table != NULL) {
    g_hash_table_destroy (priv->source_table);
  }

  if (priv->timeline_objects_table != NULL) {
    g_hash_table_foreach (priv->timeline_objects_table,
        (GHFunc) list_table_destroyer, NULL);
  }

  if (priv->layers_table != NULL)
    g_hash_table_destroy (priv->layers_table);

  if (priv->track_objects_table != NULL) {
    g_hash_table_destroy (priv->track_objects_table);
  }

  G_OBJECT_CLASS (ges_pitivi_formatter_parent_class)->finalize (object);
}

GESPitiviFormatter *
ges_pitivi_formatter_new (void)
{
  return g_object_new (GES_TYPE_PITIVI_FORMATTER, NULL);
}

/* Project saving functions */

gboolean
add_pitivi_source (GESFormatter * self, const gchar * source, const gchar * uri)
{
  xmlDocPtr doc;
  xmlNodeSetPtr nodes;
  xmlXPathObjectPtr xpathObj;
  xmlNodePtr parent;
  xmlNodePtr newline;
  xmlNodePtr new;
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;

  if (!(doc = xmlParseFile (uri))) {
    GST_ERROR ("The xptv file for uri %s was badly formed or did not exist",
        uri);
    return FALSE;
  }

  xmlKeepBlanksDefault (0);

  priv->xpathCtx = xmlXPathNewContext (doc);

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/factories/sources", priv->xpathCtx);

  xpathObj = xpathObj;
  nodes = xpathObj->nodesetval;
  parent = nodes->nodeTab[0];

  new = xmlNewText ((xmlChar *) " ");
  xmlAddChild (parent, new);

  new = xmlNewNode (0, (xmlChar *) "unused_source");
  xmlNodeSetContent (new, (xmlChar *) source);

  xmlAddChild (parent, new);

  newline = xmlNewText ((xmlChar *) "\n  ");
  xmlAddNextSibling (new, newline);

  xmlXPathFreeContext (priv->xpathCtx);
  xmlSaveFormatFile (uri, doc, 1);
  xmlFreeDoc (doc);

  return TRUE;
}

static void inline
write_int_attribute (xmlTextWriterPtr writer, guint64 nb, const gchar * attr,
    const gchar * type)
{
  gchar *str = g_strdup_printf ("%s%" PRIu64, type, nb);
  xmlTextWriterWriteAttribute (writer, BAD_CAST attr, BAD_CAST str);
  g_free (str);
}

static void
save_track_objects (xmlTextWriterPtr writer, GList * source_list,
    GESTrackType type, gint * id)
{
  GList *tmp, *tck_objs, *tmp_tck;
  gchar *bin_desc;
  xmlTextWriterStartElement (writer, BAD_CAST "track-objects");

  GST_DEBUG ("Saving track objects");
  for (tmp = source_list; tmp; tmp = tmp->next) {
    SrcMapping *srcmap;
    GESTimelineObject *object;
    guint i, j;
    guint64 inpoint, duration, start;

    srcmap = (SrcMapping *) tmp->data;
    object = srcmap->obj;

    /* Save track associated objects */
    tck_objs = ges_timeline_object_get_track_objects (object);
    for (tmp_tck = tck_objs; tmp_tck; tmp_tck = tmp_tck->next) {
      xmlChar *cast;
      GESTrackObject *tckobj = GES_TRACK_OBJECT (tmp_tck->data);
      GESTrack *track = ges_track_object_get_track (tckobj);
      const gchar *active, *locked;

      if (!track) {
        GST_WARNING ("Track object %p not in a track yet", tckobj);
        continue;
      }

      /* We are serializing this track type */
      if (track->type != type)
        continue;

      /* Save properties */
      /* Set important properties */
      xmlTextWriterStartElement (writer, BAD_CAST "track-object");

      active =
          ges_track_object_is_active (tckobj) ? "(bool)True" : "(bool)False";
      xmlTextWriterWriteAttribute (writer, BAD_CAST "active", BAD_CAST active);
      locked =
          ges_track_object_is_locked (tckobj) ? "(bool)True" : "(bool)False";
      xmlTextWriterWriteAttribute (writer, BAD_CAST "locked", BAD_CAST locked);

      /*  Here the priority correspond to the layer priority */
      write_int_attribute (writer, srcmap->priority, "priority", "(int)");
      g_object_get (G_OBJECT (tckobj), "duration", &duration, "start", &start,
          "in-point", &inpoint, NULL);
      write_int_attribute (writer, duration, "duration", "(gint64)");
      write_int_attribute (writer, start, "start", "(gint64)");
      write_int_attribute (writer, inpoint, "in_point", "(gint64)");

      cast = xmlXPathCastNumberToString (*id);
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST cast);
      xmlFree (cast);

      if (GES_IS_TRACK_EFFECT (tckobj)) {
        GParamSpec **pspecs, *spec;
        gchar *serialized, *concatenated;
        guint n_props = 0;

        xmlTextWriterWriteAttribute (writer, BAD_CAST "type",
            BAD_CAST "pitivi.timeline.track.TrackEffect");

        g_object_get (tckobj, "bin-description", &bin_desc, NULL);
        xmlTextWriterStartElement (writer, BAD_CAST "effect");
        xmlTextWriterStartElement (writer, BAD_CAST "factory");
        xmlTextWriterWriteAttribute (writer, BAD_CAST "name",
            BAD_CAST bin_desc);
        xmlTextWriterEndElement (writer);
        xmlTextWriterStartElement (writer, BAD_CAST "gst-element-properties");

        pspecs = ges_track_object_list_children_properties (tckobj, &n_props);

        j = 0;

        while (j < n_props) {
          GValue val = { 0 };

          spec = pspecs[j];
          g_value_init (&val, spec->value_type);
          ges_track_object_get_child_property_by_pspec (tckobj, spec, &val);
          serialized = gst_value_serialize (&val);
          if (!g_strcmp0 (spec->name, (gchar *) "preset")) {
            concatenated =
                g_strconcat ("(GEnum)",
                xmlXPathCastNumberToString ((g_value_get_enum (&val))), NULL);
            xmlTextWriterWriteAttribute (writer, BAD_CAST spec->name,
                BAD_CAST concatenated);
          } else {
            concatenated =
                g_strconcat ("(", g_type_name (spec->value_type), ")",
                serialized, NULL);
            xmlTextWriterWriteAttribute (writer, BAD_CAST spec->name,
                BAD_CAST concatenated);
          }
          j++;
        }

        xmlTextWriterEndElement (writer);

        for (i = 0; i < n_props; i++) {
          g_param_spec_unref (pspecs[i]);
        }

        g_free (pspecs);

      } else {
        xmlTextWriterWriteAttribute (writer, BAD_CAST "type",
            BAD_CAST "pitivi.timeline.track.SourceTrackObject");

        xmlTextWriterStartElement (writer, BAD_CAST "factory-ref");
        xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
            BAD_CAST srcmap->id);
      }
      xmlTextWriterEndElement (writer);
      xmlTextWriterEndElement (writer);

      /* We add effects at the end of the trackobject list */
      if (GES_IS_TRACK_EFFECT (tckobj)) {
        srcmap->tck_obj_ids = g_list_append (srcmap->tck_obj_ids,
            xmlXPathCastNumberToString (*id));
      } else {
        srcmap->tck_obj_ids = g_list_prepend (srcmap->tck_obj_ids,
            xmlXPathCastNumberToString (*id));
      }
      *id = *id + 1;
    }
  }

  xmlTextWriterEndElement (writer);
}

static void
save_tracks (GESTimeline * timeline, xmlTextWriterPtr writer,
    GList * source_list)
{
  GList *tracks, *tmp;

  gint id = 0;

  xmlTextWriterStartElement (writer, BAD_CAST "timeline");
  xmlTextWriterStartElement (writer, BAD_CAST "tracks");

  GST_DEBUG ("Saving tracks");

  tracks = ges_timeline_get_tracks (timeline);
  for (tmp = tracks; tmp; tmp = tmp->next) {
    gchar *caps;
    GESTrackType type;

    GESTrack *track = GES_TRACK (tmp->data);

    xmlTextWriterStartElement (writer, BAD_CAST "track");
    xmlTextWriterStartElement (writer, BAD_CAST "stream");

    /* Serialize track type and caps */
    g_object_get (G_OBJECT (track), "track-type", &type, NULL);
    caps = gst_caps_to_string (ges_track_get_caps (track));
    xmlTextWriterWriteAttribute (writer, BAD_CAST "caps", BAD_CAST caps);
    g_free (caps);

    if (type == GES_TRACK_TYPE_AUDIO) {
      xmlTextWriterWriteAttribute (writer, BAD_CAST "type",
          BAD_CAST "pitivi.stream.AudioStream");
      xmlTextWriterEndElement (writer);
    } else if (type == GES_TRACK_TYPE_VIDEO) {
      xmlTextWriterWriteAttribute (writer, BAD_CAST "type",
          BAD_CAST "pitivi.stream.VideoStream");
      xmlTextWriterEndElement (writer);
    } else {
      GST_WARNING ("Track type %i not supported", type);

      continue;
    }

    save_track_objects (writer, source_list, type, &id);
    xmlTextWriterEndElement (writer);
  }

  g_list_free_full (tracks, g_object_unref);
  xmlTextWriterEndElement (writer);
}

static GList *
save_sources (GList * layers, xmlTextWriterPtr writer)
{
  GList *tlobjects, *tmp, *tmplayer;
  GESTimelineLayer *layer;
  GHashTable *source_table;

  GList *source_list = NULL;
  int id = 1;

  GST_DEBUG ("Saving sources");

  source_table =
      g_hash_table_new_full (g_str_hash, g_int_equal, g_free, g_free);

  for (tmplayer = layers; tmplayer; tmplayer = tmplayer->next) {
    layer = GES_TIMELINE_LAYER (tmplayer->data);

    tlobjects = ges_timeline_layer_get_objects (layer);
    for (tmp = tlobjects; tmp; tmp = tmp->next) {
      SrcMapping *srcmap = g_slice_new0 (SrcMapping);
      GESTimelineObject *tlobj;
      gchar *tfs_uri;
      tlobj = tmp->data;

      if (GES_IS_TIMELINE_FILE_SOURCE (tlobj)) {

        tfs_uri = (gchar *) ges_timeline_filesource_get_uri
            (GES_TIMELINE_FILE_SOURCE (tlobj));

        if (!g_hash_table_lookup (source_table, tfs_uri)) {
          gchar *strid = g_strdup_printf ("%i", id);

          g_hash_table_insert (source_table, g_strdup (tfs_uri), strid);
          xmlTextWriterStartElement (writer, BAD_CAST "source");
          xmlTextWriterWriteAttribute (writer, BAD_CAST "filename",
              BAD_CAST tfs_uri);
          xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST strid);
          xmlTextWriterEndElement (writer);
          id++;
        }

        srcmap->id = g_strdup (g_hash_table_lookup (source_table, tfs_uri));
        srcmap->obj = g_object_ref (tlobj);
        srcmap->priority = ges_timeline_layer_get_priority (layer);
        /* We fill up the tck_obj_ids in save_track_objects */
        source_list = g_list_append (source_list, srcmap);
      }
    }
    g_list_free_full (tlobjects, g_object_unref);
    g_object_unref (G_OBJECT (layer));
  }
  g_hash_table_destroy (source_table);

  return source_list;
}

static void
save_timeline_objects (xmlTextWriterPtr writer, GList * list)
{
  GList *tmp, *tck_obj_ids;

  xmlTextWriterStartElement (writer, BAD_CAST "timeline-objects");

  GST_DEBUG ("Saving timeline objects");

  for (tmp = list; tmp; tmp = tmp->next) {

    SrcMapping *srcmap = (SrcMapping *) tmp->data;

    xmlTextWriterStartElement (writer, BAD_CAST "timeline-object");
    xmlTextWriterStartElement (writer, BAD_CAST "factory-ref");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST srcmap->id);
    xmlTextWriterEndElement (writer);
    xmlTextWriterStartElement (writer, BAD_CAST "track-object-refs");

    for (tck_obj_ids = srcmap->tck_obj_ids; tck_obj_ids;
        tck_obj_ids = tck_obj_ids->next) {
      xmlTextWriterStartElement (writer, BAD_CAST "track-object-ref");
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST tck_obj_ids->data);
      xmlTextWriterEndElement (writer);
    }
    xmlTextWriterEndElement (writer);
    xmlTextWriterEndElement (writer);
  }
  xmlTextWriterEndElement (writer);
}

static gboolean
save_pitivi_timeline_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri)
{
  xmlTextWriterPtr writer;
  GList *list = NULL, *layers = NULL;

  writer = xmlNewTextWriterFilename (uri, 0);
  xmlTextWriterSetIndent (writer, 1);
  xmlTextWriterStartElement (writer, BAD_CAST "pitivi");
  xmlTextWriterWriteAttribute (writer, BAD_CAST "formatter", BAD_CAST "GES");
  /*  */
  xmlTextWriterWriteAttribute (writer, BAD_CAST "version", BAD_CAST VERSION);

  xmlTextWriterStartElement (writer, BAD_CAST "factories");
  xmlTextWriterStartElement (writer, BAD_CAST "sources");

  layers = ges_timeline_get_layers (timeline);
  list = save_sources (layers, writer);

  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);

  save_tracks (timeline, writer, list);
  save_timeline_objects (writer, list);
  xmlTextWriterEndDocument (writer);
  xmlFreeTextWriter (writer);

  g_list_free (layers);
  g_list_foreach (list, (GFunc) free_src_map, NULL);
  g_list_free (list);

  return TRUE;
}

GList *
ges_pitivi_formatter_get_sources (GESPitiviFormatter * formatter, gchar * uri)
{
  GList *source_list = NULL;
  xmlXPathContextPtr xpathCtx;
  xmlDocPtr doc;
  xmlXPathObjectPtr xpathObj;
  int size, j;
  xmlNodeSetPtr nodes;

  if (!(doc = xmlParseFile (uri))) {
    GST_ERROR ("The xptv file for uri %s was badly formed or did not exist",
        uri);
    return FALSE;
  }

  xpathCtx = xmlXPathNewContext (doc);

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/factories/sources/source", xpathCtx);

  nodes = xpathObj->nodesetval;

  size = (nodes) ? nodes->nodeNr : 0;
  for (j = 0; j < size; ++j) {
    xmlAttr *cur_attr;
    gchar *name, *value;
    xmlNodePtr node;

    node = nodes->nodeTab[j];
    for (cur_attr = node->properties; cur_attr; cur_attr = cur_attr->next) {
      name = (gchar *) cur_attr->name;
      value = (gchar *) xmlGetProp (node, cur_attr->name);
      if (!g_strcmp0 (name, (gchar *) "filename"))
        source_list = g_list_append (source_list, g_strdup (value));
      xmlFree (value);
    }
  }

  xmlXPathFreeObject (xpathObj);

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/factories/sources/unused_source", xpathCtx);

  nodes = xpathObj->nodesetval;

  size = (nodes) ? nodes->nodeNr : 0;
  for (j = 0; j < size; ++j) {
    xmlNodePtr node;

    node = nodes->nodeTab[j];
    source_list =
        g_list_append (source_list,
        g_strdup ((gchar *) xmlNodeGetContent (node)));
  }

  xmlXPathFreeObject (xpathObj);

  xmlXPathFreeContext (xpathCtx);
  xmlFreeDoc (doc);
  return source_list;
}

/* Project loading functions */

/* Return: a GHashTable containing:
 *    {attr: value}
 */
static GHashTable *
get_nodes_infos (xmlNodePtr node)
{
  xmlAttr *cur_attr;
  GHashTable *props_table;
  gchar *name, *value;

  props_table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

  for (cur_attr = node->properties; cur_attr; cur_attr = cur_attr->next) {
    name = (gchar *) cur_attr->name;
    value = (gchar *) xmlGetProp (node, cur_attr->name);
    g_hash_table_insert (props_table, g_strdup (name), g_strdup (value));
    xmlFree (value);
  }

  return props_table;
}

static gboolean
create_tracks (GESFormatter * self)
{
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;
  GList *tracks = NULL;

  tracks = ges_timeline_get_tracks (priv->timeline);

  GST_DEBUG ("Creating tracks, current number of tracks",
      g_list_length (tracks));

  if (tracks) {
    GList *tmp = NULL;
    GESTrack *track;
    for (tmp = tracks; tmp; tmp = tmp->next) {
      track = tmp->data;
      if (track->type == GES_TRACK_TYPE_AUDIO) {
        priv->tracka = track;
      } else {
        priv->trackv = track;
      }
    }
    g_list_free_full (tracks, g_object_unref);
    return TRUE;
  }

  priv->tracka = ges_track_audio_raw_new ();
  priv->trackv = ges_track_video_raw_new ();

  if (!ges_timeline_add_track (priv->timeline, priv->trackv)) {
    return FALSE;
  }

  if (!ges_timeline_add_track (priv->timeline, priv->tracka)) {
    return FALSE;
  }

  return TRUE;
}

static GHashTable *
list_sources (GESFormatter * self)
{
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;
  xmlXPathObjectPtr xpathObj;
  GHashTable *table, *sources_table;
  int size, j;
  gchar *id;
  xmlNodeSetPtr nodes;

  sources_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) g_hash_table_destroy);

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/factories/sources/source", priv->xpathCtx);
  nodes = xpathObj->nodesetval;

  size = (nodes) ? nodes->nodeNr : 0;
  for (j = 0; j < size; ++j) {
    table = get_nodes_infos (nodes->nodeTab[j]);
    id = (gchar *) g_hash_table_lookup (table, (gchar *) "id");
    g_hash_table_insert (sources_table, g_strdup (id), table);
  }

  xmlXPathFreeObject (xpathObj);
  return sources_table;
}

static gboolean
parse_track_objects (GESFormatter * self)
{
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  int size, j;
  gchar *id, *fac_ref;
  GHashTable *table = NULL, *effect_table = NULL;
  xmlNode *first_child;
  gchar *media_type;

  /* FIXME Make this whole function cleaner starting from
   * "/pitivi/timeline/tracks/track/stream" and descending
   * into the children. */
  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/tracks/track/track-objects/track-object",
      priv->xpathCtx);

  if (xpathObj == NULL) {
    GST_DEBUG ("No track object found");

    return FALSE;
  }

  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;

  for (j = 0; j < size; ++j) {
    xmlNodePtr node = nodes->nodeTab[j];

    table = get_nodes_infos (nodes->nodeTab[j]);
    id = (gchar *) g_hash_table_lookup (table, (gchar *) "id");
    first_child = nodes->nodeTab[j]->children->next;
    fac_ref = (gchar *) xmlGetProp (first_child, (xmlChar *) "id");

    /* We check if the first child is "effect" */
    if (!g_strcmp0 ((gchar *) first_child->name, (gchar *) "effect")) {
      xmlChar *effect_name;
      xmlNodePtr fact_node = first_child->children->next;

      /* We have a node called "text" in between thus ->next->next */
      xmlNodePtr elem_props_node = fact_node->next->next;

      effect_name = xmlGetProp (fact_node, (xmlChar *) "name");
      g_hash_table_insert (table, g_strdup ((gchar *) "effect_name"),
          g_strdup ((gchar *) effect_name));
      xmlFree (effect_name);

      /* We put the effects properties in an hacktable (Lapsus is on :) */
      effect_table = get_nodes_infos (elem_props_node);

      g_hash_table_insert (table, g_strdup ((gchar *) "fac_ref"),
          g_strdup ("effect"));

      xmlFree (fac_ref);
    } else {

      g_hash_table_insert (table, g_strdup ((gchar *) "fac_ref"),
          g_strdup (fac_ref));
      xmlFree (fac_ref);
    }

    /* Same as before, we got a text node in between, thus the 2 prev
     * node->parent is <track-objects>, the one before is <stream>
     */
    media_type = (gchar *) xmlGetProp (node->parent->prev->prev,
        (const xmlChar *) "type");
    g_hash_table_insert (table, g_strdup ((gchar *) "media_type"),
        g_strdup (media_type));
    xmlFree (media_type);


    if (effect_table)
      g_hash_table_insert (table, g_strdup ("effect_props"), effect_table);

    g_hash_table_insert (priv->track_objects_table, g_strdup (id), table);
  }

  xmlXPathFreeObject (xpathObj);
  return TRUE;
}

static gboolean
parse_timeline_objects (GESFormatter * self)
{
  int size, j;
  xmlNodeSetPtr nodes;
  xmlXPathObjectPtr xpathObj;
  xmlNodePtr tlobj_nd, tmp_nd, tmp_nd2;
  xmlChar *tckobjrefId, *facrefId = NULL;

  GList *reflist = NULL;
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;
  GHashTable *tlobjs_table = priv->timeline_objects_table;

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/timeline-objects/timeline-object", priv->xpathCtx);

  if (xpathObj == NULL) {
    xmlXPathFreeObject (xpathObj);
    return FALSE;
  }

  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;

  for (j = 0; j < size; j++) {
    tlobj_nd = nodes->nodeTab[j];

    for (tmp_nd = tlobj_nd->children; tmp_nd; tmp_nd = tmp_nd->next) {
      /* We assume that factory-ref is always before the tckobjs-ref */
      if (!xmlStrcmp (tmp_nd->name, (xmlChar *) "factory-ref")) {
        facrefId = xmlGetProp (tmp_nd, (xmlChar *) "id");

      } else if (!xmlStrcmp (tmp_nd->name, (xmlChar *) "track-object-refs")) {

        for (tmp_nd2 = tmp_nd->children; tmp_nd2; tmp_nd2 = tmp_nd2->next) {
          if (!xmlStrcmp (tmp_nd2->name, (xmlChar *) "track-object-ref")) {
            /* We add the track object ref ID to the list of the current
             * TimelineObject tracks, this way we can merge 2
             * TimelineObject-s into 1 when we have unlinked TrackObject-s */
            reflist = g_hash_table_lookup (tlobjs_table, facrefId);
            tckobjrefId = xmlGetProp (tmp_nd2, (xmlChar *) "id");
            reflist = g_list_append (reflist, g_strdup ((gchar *) tckobjrefId));
            g_hash_table_insert (tlobjs_table, g_strdup ((gchar *) facrefId),
                reflist);

            xmlFree (tckobjrefId);
          }
        }
      }
    }
  }

  xmlXPathFreeObject (xpathObj);
  return TRUE;
}

static void
set_properties (GObject * obj, GHashTable * props_table)
{
  gint i;
  gchar **prop_array, *valuestr;
  gint64 value;

  gchar props[3][10] = { "duration", "in_point", "start" };

  for (i = 0; i < 3; i++) {
    valuestr = g_hash_table_lookup (props_table, props[i]);
    prop_array = g_strsplit (valuestr, ")", 0);
    value = g_ascii_strtoll (prop_array[1], NULL, 0);
    g_object_set (obj, props[i], value, NULL);

    g_strfreev (prop_array);
  }
}

static void
track_object_added_cb (GESTimelineObject * object,
    GESTrackObject * track_object, GHashTable * props_table)
{
  gchar *media_type = NULL, *lockedstr;
  GList *tck_objs = NULL, *tmp = NULL;
  GESTrack *track;
  gint64 start, duration;
  gboolean has_effect = FALSE, locked = TRUE;
  gint type = 0;

  tck_objs = ges_timeline_object_get_track_objects (object);
  media_type = (gchar *) g_hash_table_lookup (props_table, "media_type");
  lockedstr = (gchar *) g_hash_table_lookup (props_table, "locked");

  if (lockedstr && !g_strcmp0 (lockedstr, "(bool)False"))
    locked = FALSE;

  for (tmp = tck_objs; tmp; tmp = tmp->next) {

    if (!GES_IS_TRACK_OBJECT (tmp->data)) {
      /* If we arrive here something massively screwed */
      GST_ERROR ("Not a TrackObject, this is a bug");
      continue;
    }

    track = ges_track_object_get_track (tmp->data);
    if (!track) {
      GST_WARNING ("TrackObject not in a track yet");
      continue;
    }

    if (GES_IS_TRACK_PARSE_LAUNCH_EFFECT (tmp->data)) {
      has_effect = TRUE;
      continue;
    }

    if ((!g_strcmp0 (media_type, "pitivi.stream.VideoStream")
            && track->type == GES_TRACK_TYPE_VIDEO)
        || (!g_strcmp0 (media_type, "pitivi.stream.AudioStream")
            && track->type == GES_TRACK_TYPE_AUDIO)) {

      /* We lock the track objects so we do not move the whole TimelineObject */
      ges_track_object_set_locked (tmp->data, FALSE);
      set_properties (G_OBJECT (tmp->data), props_table);

      if (locked)
        ges_track_object_set_locked (tmp->data, TRUE);

      type = track->type;
      g_object_get (tmp->data, "start", &start, "duration", &duration, NULL);
    }
  }

  if (has_effect) {
    tck_objs = ges_timeline_object_get_track_objects (object);

    /* FIXME make sure this is te way we want to handle that
     * ie: set duration and start as the other trackobject
     * and no let full control to the user. */

    for (tmp = tck_objs; tmp; tmp = tmp->next) {
      /* We set the effects start and duration */
      track = ges_track_object_get_track (tmp->data);

      if (GES_IS_TRACK_PARSE_LAUNCH_EFFECT (tmp->data)
          && (type == track->type)) {
        /* We lock the track objects so we do not move the whole TimelineObject */
        ges_track_object_set_locked (tmp->data, FALSE);
        g_object_set (tmp->data, "start", start, "duration", duration, NULL);
        if (locked)
          ges_track_object_set_locked (tmp->data, TRUE);
      }
    }
  }

}

static void
make_source (GESFormatter * self, GList * reflist, GHashTable * source_table)
{
  GHashTable *props_table, *effect_table;
  gchar **prio_array;
  GESTimelineLayer *layer;
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;

  gchar *fac_ref = NULL, *media_type = NULL, *filename = NULL, *prio_str;
  GList *tmp = NULL, *keys, *tmp_key;
  GESTimelineFileSource *src = NULL;
  gint prio;
  gboolean a_avail = FALSE, v_avail = FALSE, video;
  GHashTable *tckobj_table = priv->track_objects_table;

  for (tmp = reflist; tmp; tmp = tmp->next) {

    /* Get the layer */
    props_table = g_hash_table_lookup (tckobj_table, (gchar *) tmp->data);
    prio_str = (gchar *) g_hash_table_lookup (props_table, "priority");
    prio_array = g_strsplit (prio_str, ")", 0);
    prio = (gint) g_ascii_strtod (prio_array[1], NULL);
    g_strfreev (prio_array);

    /* If we do not have any layer with this priority, create it */
    if (!(layer = g_hash_table_lookup (priv->layers_table, &prio))) {
      layer = ges_timeline_layer_new ();
      g_object_set (layer, "auto-transition", TRUE, "priority", prio, NULL);
      ges_timeline_add_layer (priv->timeline, layer);
      g_hash_table_insert (priv->layers_table, g_memdup (&prio,
              sizeof (guint64)), layer);
    }

    fac_ref = (gchar *) g_hash_table_lookup (props_table, "fac_ref");
    media_type = (gchar *) g_hash_table_lookup (props_table, "media_type");

    if (!g_strcmp0 (media_type, "pitivi.stream.VideoStream"))
      video = TRUE;
    else
      video = FALSE;

    /* FIXME I am sure we could reimplement this whole part
     * in a simpler way */
    if (g_strcmp0 (fac_ref, (gchar *) "effect")) {
      if (a_avail && (!video)) {
        a_avail = FALSE;
        g_signal_connect (src, "track-object-added",
            G_CALLBACK (track_object_added_cb), props_table);
      } else if (v_avail && (video)) {
        v_avail = FALSE;
        g_signal_connect (src, "track-object-added",
            G_CALLBACK (track_object_added_cb), props_table);

      } else {

        /* If we only have audio or only video in the previous source,
         * set it has such */
        if (a_avail) {
          ges_timeline_filesource_set_supported_formats (src,
              GES_TRACK_TYPE_VIDEO);
        } else if (v_avail) {
          ges_timeline_filesource_set_supported_formats (src,
              GES_TRACK_TYPE_AUDIO);
        }

        filename = (gchar *) g_hash_table_lookup (source_table, "filename");

        src = ges_timeline_filesource_new (filename);

        if (!video) {
          v_avail = TRUE;
          a_avail = FALSE;
        } else {
          a_avail = TRUE;
          v_avail = FALSE;
        }
        set_properties (G_OBJECT (src), props_table);
        ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (src));
      }

    } else {
      GESTrackParseLaunchEffect *effect;
      gchar *active = (gchar *) g_hash_table_lookup (props_table, "active");

      effect = ges_track_parse_launch_effect_new ((gchar *)
          g_hash_table_lookup (props_table, (gchar *) "effect_name"));
      effect_table =
          g_hash_table_lookup (props_table, (gchar *) "effect_props");

      ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT (src),
          GES_TRACK_OBJECT (effect));

      if (!g_strcmp0 (active, (gchar *) "(bool)False"))
        ges_track_object_set_active (GES_TRACK_OBJECT (effect), FALSE);

      if (video)
        ges_track_add_object (priv->trackv, GES_TRACK_OBJECT (effect));
      else
        ges_track_add_object (priv->tracka, GES_TRACK_OBJECT (effect));

      /* Set effect properties */
      keys = g_hash_table_get_keys (effect_table);
      for (tmp_key = keys; tmp_key; tmp_key = tmp_key->next) {
        GstStructure *structure;
        const GValue *value;
        GParamSpec *spec;
        GstCaps *caps;
        gchar *prop_val;

        prop_val = (gchar *) g_hash_table_lookup (effect_table,
            (gchar *) tmp_key->data);

        if (g_strstr_len (prop_val, -1, "(GEnum)")) {
          gchar **val = g_strsplit (prop_val, ")", 2);

          ges_track_object_set_child_property (GES_TRACK_OBJECT (effect),
              (gchar *) tmp_key->data, atoi (val[1]), NULL);
          g_strfreev (val);

        } else if (ges_track_object_lookup_child (GES_TRACK_OBJECT (effect),
                (gchar *) tmp->data, NULL, &spec)) {
          gchar *caps_str = g_strdup_printf ("structure1, property1=%s;",
              prop_val);

          caps = gst_caps_from_string (caps_str);
          g_free (caps_str);
          structure = gst_caps_get_structure (caps, 0);
          value = gst_structure_get_value (structure, "property1");

          ges_track_object_set_child_property_by_pspec (GES_TRACK_OBJECT
              (effect), spec, (GValue *) value);
          gst_caps_unref (caps);
        }
      }
    }
  }

  if (a_avail) {
    ges_timeline_filesource_set_supported_formats (src, GES_TRACK_TYPE_VIDEO);
  } else if (v_avail) {
    ges_timeline_filesource_set_supported_formats (src, GES_TRACK_TYPE_AUDIO);
  }
}

static gboolean
make_timeline_objects (GESFormatter * self)
{
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;
  GHashTable *source_table;

  GList *keys = NULL, *tmp = NULL, *reflist = NULL;

  keys = g_hash_table_get_keys (priv->timeline_objects_table);

  for (tmp = keys; tmp; tmp = tmp->next) {
    gchar *fac_id = (gchar *) tmp->data;

    reflist = g_hash_table_lookup (priv->timeline_objects_table, fac_id);
    source_table = g_hash_table_lookup (priv->source_table, fac_id);
    make_source (self, reflist, source_table);
  }

  g_list_free (keys);
  return TRUE;
}

static gboolean
load_pitivi_file_from_uri (GESFormatter * self,
    GESTimeline * timeline, const gchar * uri)
{
  xmlDocPtr doc;
  GESTimelineLayer *layer;
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;

  gboolean ret = TRUE;
  gint *prio = malloc (sizeof (gint));

  *prio = 0;
  layer = ges_timeline_layer_new ();
  g_object_set (layer, "auto-transition", TRUE, NULL);

  g_hash_table_insert (priv->layers_table, prio, layer);
  priv->timeline = timeline;
  g_object_set (layer, "priority", (gint32) 0, NULL);

  if (!ges_timeline_add_layer (timeline, layer)) {
    GST_ERROR ("Couldn't add layer");
    return FALSE;
  }

  if (!(doc = xmlParseFile (uri))) {
    GST_ERROR ("The xptv file for uri %s was badly formed or did not exist",
        uri);
    return FALSE;
  }

  priv->xpathCtx = xmlXPathNewContext (doc);

  if (!create_tracks (self)) {
    GST_ERROR ("Couldn't create tracks");
    return FALSE;
  }

  priv->source_table = list_sources (self);

  if (!parse_timeline_objects (self)) {
    GST_ERROR ("Couldn't find timeline objects markup in the xptv file");
    return FALSE;
  }

  if (!parse_track_objects (self)) {
    GST_ERROR ("Couldn't find track objects markup in the xptv file");
    return FALSE;
  }

  if (!make_timeline_objects (self)) {
    GST_ERROR ("Couldn't deserialise the project properly");
    return FALSE;
  }

  xmlXPathFreeContext (priv->xpathCtx);
  xmlFreeDoc (doc);
  return ret;
}
