/* GStreamer Editing Services
 * Copyright (C) 2010 Edward Hervey <bilboed@bilboed.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <ges/ges.h>
#include <gst/pbutils/encoding-profile.h>

#include <locale.h>             /* for LC_ALL */
#include "ges-validate.h"
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

/* GLOBAL VARIABLE */
static guint repeat = 0;
static gboolean mute = FALSE;
static gboolean disable_mixing = FALSE;
static GESPipeline *pipeline = NULL;
static gboolean seenerrors = FALSE;
static gchar *save_path = NULL;
static GMainLoop *mainloop;
static GESTrackType track_types = GES_TRACK_TYPE_AUDIO | GES_TRACK_TYPE_VIDEO;
static GESTimeline *timeline;
static gboolean needs_set_state;
static const gchar *scenario = NULL;

#ifdef G_OS_UNIX
static gboolean
intr_handler (gpointer user_data)
{
  g_print ("interrupt received.\n");

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "ges-launch.interupted");

  g_main_loop_quit (mainloop);

  /* remove signal handler */
  return TRUE;
}
#endif /* G_OS_UNIX */

static gchar *
ensure_uri (gchar * location)
{
  if (gst_uri_is_valid (location))
    return g_strdup (location);
  else
    return gst_filename_to_uri (location, NULL);
}

static guint
get_flags_from_string (GType type, const gchar * str_flags)
{
  guint i;
  gint flags = 0;
  GFlagsClass *class = g_type_class_ref (type);

  for (i = 0; i < class->n_values; i++) {
    if (g_strrstr (str_flags, class->values[i].value_nick)) {
      flags |= class->values[i].value;
    }
  }
  g_type_class_unref (class);

  return flags;
}



static gboolean
parse_track_type (const gchar * option_name, const gchar * value,
    gpointer udata, GError ** error)
{
  track_types = get_flags_from_string (GES_TYPE_TRACK_TYPE, value);

  if (track_types == 0)
    return FALSE;

  return TRUE;
}

static gboolean
thumbnail_cb (gpointer pipeline)
{
  static int i = 0;
  GESPipeline *p = (GESPipeline *) pipeline;
  gchar *filename;
  gboolean res;

  filename = g_strdup_printf ("thumbnail%d.jpg", i++);

  res = ges_pipeline_save_thumbnail (p, -1, -1,
      (gchar *) "image/jpeg", filename, NULL);

  g_free (filename);

  return res;
}

static void
error_loading_asset_cb (GESProject * project, GError * error,
    const gchar * failed_id, GType extractable_type)
{
  g_printerr ("Error loading asset %s: %s\n", failed_id, error->message);
  seenerrors = TRUE;

  g_main_loop_quit (mainloop);
}

static void
_timeline_set_user_options (GESTimeline * timeline)
{
  GList *tmp;

retry:
  for (tmp = timeline->tracks; tmp; tmp = tmp->next) {
    if (disable_mixing) {
      GST_ERROR_OBJECT (tmp->data, "DISABLE MIXING");
      ges_track_set_mixing (tmp->data, FALSE);
    }

    if (!(GES_TRACK (tmp->data)->type & track_types)) {
      ges_timeline_remove_track (timeline, tmp->data);
      goto retry;
    }
  }
}

static void
project_loaded_cb (GESProject * project, GESTimeline * timeline)
{
  GST_INFO ("Project loaded, playing it");

  if (save_path) {
    gchar *uri;
    GError *error = NULL;

    if (g_strcmp0 (save_path, "+r") == 0) {
      uri = ges_project_get_uri (project);
    } else if (!(uri = ensure_uri (save_path))) {
      g_error ("couldn't create uri for '%s", save_path);

      seenerrors = TRUE;
      g_main_loop_quit (mainloop);
    }

    g_print ("\nSaving project to %s\n", uri);
    ges_project_save (project, timeline, uri, NULL, TRUE, &error);
    g_free (uri);

    g_assert_no_error (error);
    if (error) {
      seenerrors = TRUE;
      g_main_loop_quit (mainloop);
    }
  }

  _timeline_set_user_options (timeline);

  if (ges_validate_activate (GST_PIPELINE (pipeline), scenario,
          &needs_set_state) == FALSE) {
    g_error ("Could not activate scenario %s", scenario);
    seenerrors = TRUE;
    g_main_loop_quit (mainloop);
  }

  if (needs_set_state && gst_element_set_state (GST_ELEMENT (pipeline),
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to start the pipeline\n");
  }
}


static gboolean
_save_timeline (GESTimeline * timeline, const gchar * load_path)
{
  if (save_path && !load_path) {
    gchar *uri;
    if (!(uri = ensure_uri (save_path))) {
      g_error ("couldn't create uri for '%s", save_path);
      return FALSE;
    }

    return ges_timeline_save_to_uri (timeline, uri, NULL, TRUE, NULL);
  }

  return TRUE;
}

static GESTimeline *
create_timeline (const gchar * serialized_timeline, const gchar * proj_uri,
    const gchar * scenario)
{
  GESTimeline *timeline;
  GESProject *project;

  GError *error = NULL;

  if (proj_uri != NULL) {
    project = ges_project_new (proj_uri);
  } else {
    project = ges_project_new (serialized_timeline);
  }

  g_signal_connect (project, "error-loading-asset",
      G_CALLBACK (error_loading_asset_cb), NULL);
  g_signal_connect (project, "loaded", G_CALLBACK (project_loaded_cb), NULL);

  timeline = GES_TIMELINE (ges_asset_extract (GES_ASSET (project), &error));

  if (error) {
    g_error ("Could not create timeline, error: %s", error->message);

    return NULL;
  }

  return timeline;
}

static GESPipeline *
create_pipeline (GESTimeline ** ret_timeline, gchar * load_path,
    const gchar * serialized_timeline, const gchar * scenario)
{
  gchar *uri = NULL;
  GESTimeline *timeline = NULL;

  /* Timeline creation */
  if (load_path) {
    g_printf ("Loading project from : %s\n", load_path);

    if (!(uri = ensure_uri (load_path))) {
      g_error ("couldn't create uri for '%s'", load_path);
      goto failure;
    }
  }

  pipeline = ges_pipeline_new ();

  if (!(timeline = create_timeline (serialized_timeline, uri, scenario)))
    goto failure;

  if (!load_path)
    ges_timeline_commit (timeline);

  /* save project if path is given. we do this now in case GES crashes or
   * hangs during playback. */
  if (!_save_timeline (timeline, load_path))
    goto failure;

  /* In order to view our timeline, let's grab a convenience pipeline to put
   * our timeline in. */

  if (mute) {
    GstElement *sink = gst_element_factory_make ("fakesink", NULL);

    g_object_set (sink, "sync", TRUE, NULL);
    ges_pipeline_preview_set_audio_sink (pipeline, sink);

    sink = gst_element_factory_make ("fakesink", NULL);
    g_object_set (sink, "sync", TRUE, NULL);
    ges_pipeline_preview_set_video_sink (pipeline, sink);
  }

  /* Add the timeline to that pipeline */
  if (!ges_pipeline_set_timeline (pipeline, timeline))
    goto failure;

  *ret_timeline = timeline;

done:
  if (uri)
    g_free (uri);

  return pipeline;

failure:
  {
    if (timeline)
      gst_object_unref (timeline);
    if (pipeline)
      gst_object_unref (pipeline);
    pipeline = NULL;
    timeline = NULL;

    goto done;
  }
}

static void
bus_message_cb (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_WARNING:{
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ges-launch.warning");
      break;
    }
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg_info = NULL;

      gst_message_parse_error (message, &err, &dbg_info);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "ges-launch-error");
      g_printerr ("ERROR from element %s: %s\n", GST_OBJECT_NAME (message->src),
          err->message);
      g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
      g_error_free (err);
      g_free (dbg_info);
      seenerrors = TRUE;
      g_main_loop_quit (mainloop);
      break;
    }
    case GST_MESSAGE_EOS:
      if (repeat > 0) {
        g_printerr ("Looping again\n");
        if (!gst_element_seek_simple (GST_ELEMENT (pipeline), GST_FORMAT_TIME,
                GST_SEEK_FLAG_FLUSH, 0))
          g_printerr ("seeking failed\n");
        else
          g_printerr ("seeking succeeded\n");
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
        g_printerr ("Looping set\n");
        repeat -= 1;
      } else {
        g_printerr ("\nDone\n");
        g_main_loop_quit (mainloop);
      }
      break;
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC (message) == GST_OBJECT_CAST (pipeline)) {
        gchar *dump_name;
        GstState old, new, pending;
        gchar *state_transition_name;

        gst_message_parse_state_changed (message, &old, &new, &pending);
        state_transition_name = g_strdup_printf ("%s_%s",
            gst_element_state_get_name (old), gst_element_state_get_name (new));
        dump_name = g_strconcat ("ges-launch.", state_transition_name, NULL);


        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
            GST_DEBUG_GRAPH_SHOW_ALL, dump_name);

        g_free (dump_name);
        g_free (state_transition_name);
      }
      break;
    case GST_MESSAGE_REQUEST_STATE:
      ges_validate_handle_request_state_change (message, mainloop);
      break;
    default:
      break;
  }
}

static void
print_enum (GType enum_type)
{
  GEnumClass *enum_class = G_ENUM_CLASS (g_type_class_ref (enum_type));
  guint i;

  for (i = 0; i < enum_class->n_values; i++) {
    g_printf ("%s\n", enum_class->values[i].value_nick);
  }

  g_type_class_unref (enum_class);
}

static void
print_transition_list (void)
{
  print_enum (GES_VIDEO_STANDARD_TRANSITION_TYPE_TYPE);
}

static void
print_pattern_list (void)
{
  print_enum (GES_VIDEO_TEST_PATTERN_TYPE);
}

static GstEncodingProfile *
_parse_encoding_profile (const gchar * format)
{
  GstCaps *caps;
  GstEncodingProfile *encoding_profile = NULL;
  gchar **restriction_format, **preset_v;

  guint i = 1, presence = 0;
  GstCaps *restrictioncaps = NULL;
  gchar **strpresence_v, **strcaps_v = g_strsplit (format, ":", 0);

  if (strcaps_v[0] && *strcaps_v[0]) {
    if (strcaps_v[1] == NULL) {
      /* Only 1 profile which means no container used */
      i = 0;
    } else {
      caps = gst_caps_from_string (strcaps_v[0]);
      if (caps == NULL) {
        g_printerr ("Could not parse caps %s", strcaps_v[0]);
        return FALSE;
      }
      encoding_profile =
          GST_ENCODING_PROFILE (gst_encoding_container_profile_new
          ("User profile", "User profile", caps, NULL));
      gst_caps_unref (caps);
    }
  }

  for (; strcaps_v[i]; i++) {
    gchar *strcaps, *strpresence;
    char *preset_name = NULL;
    GstEncodingProfile *profile = NULL;

    restriction_format = g_strsplit (strcaps_v[i], "->", 0);
    if (restriction_format[1]) {
      restrictioncaps = gst_caps_from_string (restriction_format[0]);
      strcaps = g_strdup (restriction_format[1]);
    } else {
      restrictioncaps = NULL;
      strcaps = g_strdup (restriction_format[0]);
    }
    g_strfreev (restriction_format);

    preset_v = g_strsplit (strcaps, "+", 0);
    if (preset_v[1]) {
      strpresence = preset_v[1];
      g_free (strcaps);
      strcaps = g_strdup (preset_v[0]);
    } else {
      strpresence = preset_v[0];
    }

    strpresence_v = g_strsplit (strpresence, "|", 0);
    if (strpresence_v[1]) {     /* We have a presence */
      gchar *endptr;

      if (preset_v[1]) {        /* We have preset and presence */
        preset_name = g_strdup (strpresence_v[0]);
      } else {                  /* We have a presence but no preset */
        g_free (strcaps);
        strcaps = g_strdup (strpresence_v[0]);
      }

      presence = strtoll (strpresence_v[1], &endptr, 10);
      if (endptr == strpresence_v[1]) {
        g_printerr ("Wrong presence %s\n", strpresence_v[1]);

        return FALSE;
      }
    } else {                    /* We have no presence */
      if (preset_v[1]) {        /* Not presence but preset */
        preset_name = g_strdup (preset_v[1]);
        g_free (strcaps);
        strcaps = g_strdup (preset_v[0]);
      }                         /* Else we have no presence nor preset */
    }
    g_strfreev (strpresence_v);
    g_strfreev (preset_v);

    GST_DEBUG ("Creating preset with restrictions: %" GST_PTR_FORMAT
        ", caps: %s, preset %s, presence %d", restrictioncaps, strcaps,
        preset_name ? preset_name : "none", presence);

    caps = gst_caps_from_string (strcaps);
    g_free (strcaps);
    if (caps == NULL) {
      g_warning ("Could not create caps for %s", strcaps_v[i]);

      return FALSE;
    }

    if (g_str_has_prefix (strcaps_v[i], "audio/")) {
      profile = GST_ENCODING_PROFILE (gst_encoding_audio_profile_new (caps,
              preset_name, restrictioncaps, presence));
    } else if (g_str_has_prefix (strcaps_v[i], "video/") ||
        g_str_has_prefix (strcaps_v[i], "image/")) {
      profile = GST_ENCODING_PROFILE (gst_encoding_video_profile_new (caps,
              preset_name, restrictioncaps, presence));
    }

    g_free (preset_name);
    gst_caps_unref (caps);
    if (restrictioncaps)
      gst_caps_unref (restrictioncaps);

    if (profile == NULL) {
      g_warning ("No way to create a preset for caps: %s", strcaps_v[i]);

      return NULL;
    }

    if (encoding_profile) {
      if (gst_encoding_container_profile_add_profile
          (GST_ENCODING_CONTAINER_PROFILE (encoding_profile),
              profile) == FALSE) {
        g_warning ("Can not create a preset for caps: %s", strcaps_v[i]);

        return NULL;
      }
    } else {
      encoding_profile = profile;
    }
  }
  g_strfreev (strcaps_v);

  return encoding_profile;
}

static gboolean
_add_media_path (const gchar * option_name, const gchar * value,
    gpointer udata, GError ** error)
{
  g_return_val_if_fail (gst_uri_is_valid (value), FALSE);

  if (g_strcmp0 (option_name, "--sample-path-recurse") == 0 ||
      g_strcmp0 (option_name, "-R") == 0) {
    ges_add_missing_uri_relocation_uri (value, TRUE);
  } else {
    GST_INFO ("Adding folder: %s", value);
    ges_add_missing_uri_relocation_uri (value, FALSE);
  }

  return TRUE;
}

/* g_free after usage */
static gchar *
sanitize_argument (gchar * arg)
{
  char *equal_index = strstr (arg, "=");
  char *space_index = strstr (arg, " ");
  gchar *new_string = g_malloc (sizeof (gchar) * (strlen (arg) + 3));
  gchar *tmp_string = new_string;

  if (!space_index)
    return g_strdup (arg);

  if (!equal_index || equal_index > space_index)
    return g_strdup_printf ("\"%s\"", arg);

  for (arg = arg; *arg != '\0'; arg++) {
    *tmp_string = *arg;
    tmp_string += 1;
    if (*arg == '=') {
      *tmp_string = '"';
      tmp_string += 1;
    }
  }
  *tmp_string = '"';
  tmp_string += 1;
  *tmp_string = '\0';

  return new_string;
}

static gchar *
_parse_timeline (int argc, char **argv)
{
  gint i;

  gchar *string = g_strdup (" ");

  for (i = 1; i < argc; i++) {
    gchar *new_string;
    gchar *sanitized = sanitize_argument (argv[i]);

    new_string = g_strconcat (string, " ", sanitized, NULL);

    g_free (sanitized);
    g_free (string);
    string = new_string;
  }


  return string;
}

static void
_print_all_commands (void)
{
  /* Yeah I know very fancy */
  g_print ("Available ges-launch-1.0 commands:\n\n");
  g_print ("  %-8s %-10s %-10s\n\n", "clip", "effect", "set-");
  g_print ("See ges-launch-1.0 help <command> or ges-launch-1.0 help <guide> "
      "to read about a specific command or a given guide\n");
}

static void
_check_command_help (int argc, gchar ** argv)
{
  if (!g_strcmp0 (argv[1], "help")) {
    gchar *page = NULL;

    if (argc == 2)
      page = g_strdup ("ges-launch-1.0");
    else if (!g_strcmp0 (argv[2], "all"))
      _print_all_commands ();
    else
      page = g_strconcat ("ges-launch-1.0", "-", argv[2], NULL);

    if (page) {
      execlp ("man", "man", page, NULL);
      g_free (page);
    }

    /* If an error is raised by execlp it will be displayed in the terminal */
    /* TODO: implement a fallback */
    exit (0);
  }
}

int
main (int argc, gchar ** argv)
{
  gint validate_res;
  GError *err = NULL;
  gchar *outputuri = NULL, *serialized_timeline = NULL;
  const gchar *format = NULL;
  gchar *exclude_args = NULL;
  static gboolean smartrender = FALSE;
  static gboolean list_transitions = FALSE;
  static gboolean list_patterns = FALSE;
  static gdouble thumbinterval = 0;
  static gboolean verbose = FALSE;
  gchar *load_path = NULL;
  gchar *videosink = NULL, *audiosink = NULL;
  gboolean inspect_action_type = FALSE;
  gchar *encoding_profile = NULL;

  GOptionEntry options[] = {
    {"thumbnail", 'm', 0.0, G_OPTION_ARG_DOUBLE, &thumbinterval,
        "Save thumbnail every <n> seconds to current directory", "<n>"},
    {"smartrender", 's', 0, G_OPTION_ARG_NONE, &smartrender,
        "Render to outputuri and avoid decoding/reencoding", NULL},
    {"outputuri", 'o', 0, G_OPTION_ARG_STRING, &outputuri,
        "URI to encode to", "<protocol>://<location>"},
    {"format", 'f', 0, G_OPTION_ARG_STRING, &format,
          "Specify an encoding profile on the command line",
        "<profile>"},
    {"encoding-profile", 'e', 0, G_OPTION_ARG_STRING, &encoding_profile,
        "Use a specific encoding profile from XML", "<profile-name>"},
    {"repeat", 'r', 0, G_OPTION_ARG_INT, &repeat,
        "Number of times to repeat timeline", "<times>"},
    {"list-transitions", 't', 0, G_OPTION_ARG_NONE, &list_transitions,
        "List valid transition types and exit", NULL},
    {"list-patterns", 'p', 0, G_OPTION_ARG_NONE, &list_patterns,
        "List patterns and exit", NULL},
    {"save", 'z', 0, G_OPTION_ARG_STRING, &save_path,
        "Save project to file before rendering", "<path>"},
    {"load", 'l', 0, G_OPTION_ARG_STRING, &load_path,
        "Load project from file before rendering", "<path>"},
    {"verbose", 0, 0, G_OPTION_ARG_NONE, &verbose,
        "Output status information and property notifications", NULL},
    {"exclude", 'X', 0, G_OPTION_ARG_NONE, &exclude_args,
        "Do not output status information of <type>", "<type1>,<type2>,..."},
    {"track-types", 'p', 0, G_OPTION_ARG_CALLBACK, &parse_track_type,
        "Defines the track types to be created"},
    {"mute", 0, 0, G_OPTION_ARG_NONE, &mute,
        "Mute playback output by using fakesinks"},
    {"disable-mixing", 0, 0, G_OPTION_ARG_NONE, &disable_mixing,
        "Do not use mixing element in the tracks"},
    {"videosink", 'v', 0, G_OPTION_ARG_STRING, &videosink,
        "The video sink used for playing back", "<videosink>"},
    {"audiosink", 'a', 0, G_OPTION_ARG_STRING, &audiosink,
        "The audio sink used for playing back", "<audiosink>"},
    {"sample-paths", 'P', 0, G_OPTION_ARG_CALLBACK, &_add_media_path,
        "List of pathes to look assets in if they were moved"},
    {"sample-path-recurse", 'R', 0, G_OPTION_ARG_CALLBACK,
          &_add_media_path,
        "Same as above, but recursing into the folder"},
#ifdef HAVE_GST_VALIDATE
    {"inspect-action-type", 'y', 0, G_OPTION_ARG_NONE, &inspect_action_type,
          "Inspect the avalaible action types with which to write scenarios"
          " if no parameter passed, it will list all avalaible action types"
          " otherwize will print the full description of the wanted types",
        NULL},
    {"set-scenario", 0, 0, G_OPTION_ARG_STRING, &scenario,
        "Specify a GstValidate scenario to run, 'none' means load gst-validate"
          " but run no scenario on it", "<scenario_name>"},
#endif
    {NULL}
  };

  GOptionContext *ctx;
  GstBus *bus;

#ifdef G_OS_UNIX
  guint signal_watch_id;
#endif

  _check_command_help (argc, argv);
  setlocale (LC_ALL, "");

  ctx = g_option_context_new ("- plays or renders a timeline.");
  g_option_context_set_summary (ctx,
      "ges-launch renders a timeline, which can be specified on the commandline,\n"
      "or loaded from a xges file using the -l option.\n\n"
      "A timeline is a list of files, patterns, and transitions to be rendered\n"
      "one after the other. Files and Patterns provide video and audio as the\n"
      "primary input, and transitions animate between the end of one file/pattern\n"
      "and the beginning of a new one. Hence, transitions can only be listed\n"
      "in between patterns or files.\n\n"
      "A file is a triplet of filename, inpoint (in seconds) and\n"
      "duration (in seconds). If the duration is 0, the full file length is used.\n\n"
      "Patterns and transitions are triplets that begin with either \"+pattern\"\n"
      "or \"+transition\", followed by a <type> and duration (in seconds, must be\n"
      "greater than 0)\n\n"
      "Durations in all cases can be fractions of a second.\n\n"
      "========\nExamples\n========\n\n"
      "Play video1.ogv from inpoint 5 with duration 10 in seconds:\n"
      "$ ges-launch video1.ogv 5 10\n\n"
      "Crossfade:\n"
      "$ ges-launch video1.ogv 0 10 +transition crossfade 3.5 video2.ogv 0 10\n\n"
      "Render xges to ogv:\n"
      "$ ges-launch -l project.xges -o rendering.ogv\n\n"
      "Render xges to an XML encoding-profile called mymkv:\n"
      "$ ges-launch -l project.xges -o rendering.mkv -e mymkv\n\n"
      "Render to mp4:\n"
      "$ ges-launch -l project.xges -o out.mp4 \\\n"
      "             -f \"video/quicktime,variant=iso:video/x-h264:audio/mpeg,mpegversion=1,layer=3\"\n\n"
      "Render xges to WebM with 1920x1080 resolution:\n"
      "$ ges-launch -l project.xges -o out.webm \\\n"
      "             -f \"video/webm:video/x-raw,width=1920,height=1080->video/x-vp8:audio/x-vorbis\"\n\n"
      "A preset name can be used by adding +presetname:\n"
      "$ ges-launch -l project.xges -o out.webm \\\n"
      "             -f \"video/webm:video/x-vp8+presetname:x-vorbis\"\n\n"
      "The presence property of the profile can be specified with |<presence>:\n"
      "$ ges-launch -l project.xges -o out.ogv \\\n"
      "             -f \"application/ogg:video/x-theora|<presence>:audio/x-vorbis\"");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  g_option_context_add_group (ctx, ges_init_get_option_group ());

  g_option_context_set_ignore_unknown_options (ctx, TRUE);

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_printerr ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    exit (1);
  }

  /* Initialize the GStreamer Editing Services */
  if (!ges_init ()) {
    g_printerr ("Error initializing GES\n");

    exit (1);
  }

  if (list_transitions) {
    print_transition_list ();
    exit (0);
  }

  if (list_patterns) {
    print_pattern_list ();
    exit (0);
  }

  if (inspect_action_type)
    return ges_validate_print_action_types ((const gchar **) argv + 1,
        argc - 1);

  if (((!load_path && !scenario && (argc < 1)))) {
    g_printf ("%s", g_option_context_get_help (ctx, TRUE, NULL));
    g_option_context_free (ctx);
    exit (1);
  }

  g_option_context_free (ctx);

  /* Create the pipeline */
  serialized_timeline = _parse_timeline (argc, argv);
  create_pipeline (&timeline, load_path, serialized_timeline, scenario);
  if (!pipeline)
    exit (1);

  if (videosink != NULL) {
    GError *err = NULL;
    GstElement *sink = gst_parse_bin_from_description (videosink, TRUE, &err);
    if (sink == NULL) {
      GST_ERROR ("could not create the requested videosink %s (err: %s), "
          "exiting", err ? err->message : "", videosink);
      exit (1);
    }
    ges_pipeline_preview_set_video_sink (pipeline, sink);
  }

  if (audiosink != NULL) {
    GError *err = NULL;
    GstElement *sink = gst_parse_bin_from_description (audiosink, TRUE, &err);
    if (sink == NULL) {
      GST_ERROR ("could not create the requested audiosink %s (err: %s), "
          "exiting", err ? err->message : "", audiosink);
      exit (1);
    }
    ges_pipeline_preview_set_audio_sink (pipeline, sink);
  }

  /* Setup profile/encoding if needed */
  if (smartrender || outputuri) {
    GstEncodingProfile *prof = NULL;

    if (!format) {
      GESProject *proj =
          GES_PROJECT (ges_extractable_get_asset (GES_EXTRACTABLE (timeline)));
      const GList *profiles = ges_project_list_encoding_profiles (proj);

      if (profiles) {
        prof = profiles->data;
        if (encoding_profile)
          for (; profiles; profiles = profiles->next)
            if (strcmp (encoding_profile,
                    gst_encoding_profile_get_name (profiles->data)) == 0)
              prof = profiles->data;
      }
    }

    if (!prof) {
      if (format == NULL)
        format = "application/ogg:video/x-theora:audio/x-vorbis";

      prof = _parse_encoding_profile (format);
    }

    if (outputuri)
      outputuri = ensure_uri (outputuri);

    if (!prof || !ges_pipeline_set_render_settings (pipeline, outputuri, prof)
        || !ges_pipeline_set_mode (pipeline,
            smartrender ? GES_PIPELINE_MODE_SMART_RENDER :
            GES_PIPELINE_MODE_RENDER)) {
      g_free (outputuri);
      exit (1);
    }
    g_free (outputuri);

    gst_encoding_profile_unref (prof);
  } else {
    ges_pipeline_set_mode (pipeline, GES_PIPELINE_MODE_PREVIEW);
  }

  if (verbose) {
    gchar **exclude_list =
        exclude_args ? g_strsplit (exclude_args, ",", 0) : NULL;
    g_signal_connect (pipeline, "deep-notify",
        G_CALLBACK (gst_object_default_deep_notify), exclude_list);
  }

  /* Play the pipeline */
  mainloop = g_main_loop_new (NULL, FALSE);

  if (thumbinterval != 0.0) {
    g_printf ("thumbnailing every %f seconds\n", thumbinterval);
    g_timeout_add (1000 * thumbinterval, thumbnail_cb, pipeline);
  }

  if (!load_path) {
    if (ges_validate_activate (GST_PIPELINE (pipeline), scenario,
            &needs_set_state) == FALSE) {
      g_error ("Could not activate scenario %s", scenario);
      return 29;
    }

    _timeline_set_user_options (timeline);
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), mainloop);

#ifdef G_OS_UNIX
  signal_watch_id =
      g_unix_signal_add (SIGINT, (GSourceFunc) intr_handler, pipeline);
#endif

  if (!load_path) {
    if (needs_set_state && gst_element_set_state (GST_ELEMENT (pipeline),
            GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
      g_error ("Failed to start the pipeline\n");
      return 1;
    }
  }
  g_main_loop_run (mainloop);

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);

  /*  Re save the timeline in case the scenario changed it! */
  _save_timeline (timeline, load_path);

  validate_res = ges_validate_clean (GST_PIPELINE (pipeline));
  if (seenerrors == FALSE)
    seenerrors = validate_res;

#ifdef G_OS_UNIX
  g_source_remove (signal_watch_id);
#endif

  return (int) seenerrors;
}
