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
 * SECTION:gesaudiourisource
 * @short_description: outputs a single audio stream from a given file
 */

#include "ges-utils.h"
#include "ges-internal.h"
#include "ges-track-element.h"
#include "ges-audio-uri-source.h"

struct _GESAudioUriSourcePrivate
{
  void *nothing;
};

enum
{
  PROP_0,
  PROP_URI
};

/* GESSource VMethod */
static GstElement *
ges_audio_uri_source_create_source (GESTrackElement * trksrc)
{
  GESAudioUriSource *self;
  GESTrack *track;
  GstElement *decodebin;

  self = (GESAudioUriSource *) trksrc;

  track = ges_track_element_get_track (trksrc);

  decodebin = gst_element_factory_make ("uridecodebin", NULL);

  g_object_set (decodebin, "caps", ges_track_get_caps (track),
      "expose-all-streams", FALSE, "uri", self->uri, NULL);

  return decodebin;
}

G_DEFINE_TYPE (GESAudioUriSource, ges_audio_uri_source, GES_TYPE_AUDIO_SOURCE);


/* GObject VMethods */

static void
ges_audio_uri_source_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESAudioUriSource *uriclip = GES_AUDIO_URI_SOURCE (object);

  switch (property_id) {
    case PROP_URI:
      g_value_set_string (value, uriclip->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_audio_uri_source_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GESAudioUriSource *uriclip = GES_AUDIO_URI_SOURCE (object);

  switch (property_id) {
    case PROP_URI:
      if (uriclip->uri) {
        GST_WARNING_OBJECT (object, "Uri already set to %s", uriclip->uri);
        return;
      }
      uriclip->uri = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_audio_uri_source_dispose (GObject * object)
{
  GESAudioUriSource *uriclip = GES_AUDIO_URI_SOURCE (object);

  if (uriclip->uri)
    g_free (uriclip->uri);

  G_OBJECT_CLASS (ges_audio_uri_source_parent_class)->dispose (object);
}

static void
ges_audio_uri_source_class_init (GESAudioUriSourceClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GESAudioSourceClass *source_class = GES_AUDIO_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GESAudioUriSourcePrivate));

  object_class->get_property = ges_audio_uri_source_get_property;
  object_class->set_property = ges_audio_uri_source_set_property;
  object_class->dispose = ges_audio_uri_source_dispose;

  /**
   * GESAudioUriSource:uri:
   *
   * The location of the file/resource to use.
   */
  g_object_class_install_property (object_class, PROP_URI,
      g_param_spec_string ("uri", "URI", "uri of the resource",
          NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  source_class->create_source = ges_audio_uri_source_create_source;
}

static void
ges_audio_uri_source_init (GESAudioUriSource * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_AUDIO_URI_SOURCE, GESAudioUriSourcePrivate);
}

/**
 * ges_audio_uri_source_new:
 * @uri: the URI the source should control
 *
 * Creates a new #GESAudioUriSource for the provided @uri.
 *
 * Returns: The newly created #GESAudioUriSource, or %NULL if there was an
 * error.
 */
GESAudioUriSource *
ges_audio_uri_source_new (gchar * uri)
{
  return g_object_new (GES_TYPE_AUDIO_URI_SOURCE, "uri", uri, NULL);
}
