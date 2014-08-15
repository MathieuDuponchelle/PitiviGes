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

#include "test-utils.h"
#include <ges/ges.h>
#include <gst/check/gstcheck.h>

#define LAYER_HEIGHT 1000

GST_START_TEST (test_layer_properties)
{
  GESTimeline *timeline;
  GESLayer *layer;
  GESTrack *track;
  GESTrackElement *trackelement;
  GESClip *clip;

  ges_init ();

  /* Timeline and 1 Layer */
  timeline = ges_timeline_new ();
  layer = (GESLayer *) ges_layer_new ();

  /* The default priority is 0 */
  fail_unless_equals_int (ges_layer_get_priority (layer), 0);

  /* Layers are initially floating, once we add them to the timeline,
   * the timeline will take that reference. */
  fail_unless (g_object_is_floating (layer));
  fail_unless (ges_timeline_add_layer (timeline, layer));
  fail_if (g_object_is_floating (layer));

  track = GES_TRACK (ges_video_track_new ());
  fail_unless (track != NULL);
  fail_unless (ges_timeline_add_track (timeline, track));

  clip = (GESClip *) ges_test_clip_new ();
  fail_unless (clip != NULL);

  /* Set some properties */
  g_object_set (clip, "start", (guint64) 42, "duration", (guint64) 51,
      "in-point", (guint64) 12, NULL);
  assert_equals_uint64 (_START (clip), 42);
  assert_equals_uint64 (_DURATION (clip), 51);
  assert_equals_uint64 (_INPOINT (clip), 12);
  assert_equals_uint64 (_PRIORITY (clip), 0);

  /* Add the clip to the timeline */
  fail_unless (g_object_is_floating (clip));
  fail_unless (ges_layer_add_clip (layer, GES_CLIP (clip)));
  fail_if (g_object_is_floating (clip));
  trackelement = ges_clip_find_track_element (clip, track, G_TYPE_NONE);
  fail_unless (trackelement != NULL);

  /* This is not a SimpleLayer, therefore the properties shouldn't have changed */
  assert_equals_uint64 (_START (clip), 42);
  assert_equals_uint64 (_DURATION (clip), 51);
  assert_equals_uint64 (_INPOINT (clip), 12);
  assert_equals_uint64 (_PRIORITY (clip), 0);
  ges_timeline_commit (timeline);
  nle_object_check (ges_track_element_get_nleobject (trackelement), 42, 51, 12,
      51, MIN_NLE_PRIO, TRUE);

  /* Change the priority of the layer */
  g_object_set (layer, "priority", 1, NULL);
  assert_equals_int (ges_layer_get_priority (layer), 1);
  assert_equals_uint64 (_PRIORITY (clip), 0);
  ges_timeline_commit (timeline);
  nle_object_check (ges_track_element_get_nleobject (trackelement), 42, 51, 12,
      51, LAYER_HEIGHT + MIN_NLE_PRIO, TRUE);

  /* Change it to an insanely high value */
  g_object_set (layer, "priority", 31, NULL);
  assert_equals_int (ges_layer_get_priority (layer), 31);
  assert_equals_uint64 (_PRIORITY (clip), 0);
  ges_timeline_commit (timeline);
  nle_object_check (ges_track_element_get_nleobject (trackelement), 42, 51, 12,
      51, MIN_NLE_PRIO + LAYER_HEIGHT * 31, TRUE);

  /* and back to 0 */
  g_object_set (layer, "priority", 0, NULL);
  assert_equals_int (ges_layer_get_priority (layer), 0);
  assert_equals_uint64 (_PRIORITY (clip), 0);
  ges_timeline_commit (timeline);
  nle_object_check (ges_track_element_get_nleobject (trackelement), 42, 51, 12,
      51, MIN_NLE_PRIO + 0, TRUE);

  gst_object_unref (trackelement);
  fail_unless (ges_layer_remove_clip (layer, clip));
  fail_unless (ges_timeline_remove_track (timeline, track));
  fail_unless (ges_timeline_remove_layer (timeline, layer));
  gst_object_unref (timeline);
}

GST_END_TEST;

GST_START_TEST (test_layer_priorities)
{
  GESTrack *track;
  GESTimeline *timeline;
  GESLayer *layer1, *layer2, *layer3;
  GESTrackElement *trackelement1, *trackelement2, *trackelement3;
  GESClip *clip1, *clip2, *clip3;
  GstElement *nleobj1, *nleobj2, *nleobj3;
  guint prio1, prio2, prio3;
  GList *objs, *tmp;

  ges_init ();

  /* Timeline and 3 Layer */
  timeline = ges_timeline_new ();
  layer1 = (GESLayer *) ges_layer_new ();
  layer2 = (GESLayer *) ges_layer_new ();
  layer3 = (GESLayer *) ges_layer_new ();

  ges_layer_set_priority (layer2, 1);
  ges_layer_set_priority (layer3, 2);

  fail_unless (ges_timeline_add_layer (timeline, layer1));
  fail_unless (ges_timeline_add_layer (timeline, layer2));
  fail_unless (ges_timeline_add_layer (timeline, layer3));
  fail_unless_equals_int (ges_layer_get_priority (layer1), 0);
  fail_unless_equals_int (ges_layer_get_priority (layer2), 1);
  fail_unless_equals_int (ges_layer_get_priority (layer3), 2);

  track = GES_TRACK (ges_video_track_new ());
  fail_unless (track != NULL);
  fail_unless (ges_timeline_add_track (timeline, track));

  clip1 = GES_CLIP (ges_test_clip_new ());
  ges_clip_set_supported_formats (clip1,
      GES_TRACK_TYPE_AUDIO | GES_TRACK_TYPE_VIDEO);
  clip2 = GES_CLIP (ges_test_clip_new ());
  ges_clip_set_supported_formats (clip2,
      GES_TRACK_TYPE_AUDIO | GES_TRACK_TYPE_VIDEO);
  clip3 = GES_CLIP (ges_test_clip_new ());
  ges_clip_set_supported_formats (clip3,
      GES_TRACK_TYPE_AUDIO | GES_TRACK_TYPE_VIDEO);
  fail_unless (clip1 != NULL);
  fail_unless (clip2 != NULL);
  fail_unless (clip3 != NULL);

  /* Set priorities on the objects */
  g_object_set (clip1, "priority", 0, NULL);
  assert_equals_int (_PRIORITY (clip1), 0);
  g_object_set (clip2, "priority", 1, NULL);
  assert_equals_int (_PRIORITY (clip2), 1);
  g_object_set (clip3, "priority", LAYER_HEIGHT + 1, NULL);
  assert_equals_int (_PRIORITY (clip3), LAYER_HEIGHT + 1);

  /* Add objects to the timeline */
  fail_unless (ges_layer_add_clip (layer1, clip1));
  trackelement1 = ges_clip_find_track_element (clip1, track, G_TYPE_NONE);
  fail_unless (trackelement1 != NULL);

  fail_unless (ges_layer_add_clip (layer2, clip2));
  trackelement2 = ges_clip_find_track_element (clip2, track, G_TYPE_NONE);
  fail_unless (trackelement2 != NULL);

  fail_unless (ges_layer_add_clip (layer3, clip3));
  trackelement3 = ges_clip_find_track_element (clip3, track, G_TYPE_NONE);
  fail_unless (trackelement3 != NULL);

  ges_timeline_commit (timeline);
  assert_equals_int (_PRIORITY (clip1), 0);
  nleobj1 = ges_track_element_get_nleobject (trackelement1);
  fail_unless (nleobj1 != NULL);
  g_object_get (nleobj1, "priority", &prio1, NULL);
  assert_equals_int (prio1, MIN_NLE_PRIO);

  assert_equals_int (_PRIORITY (clip2), 1);
  nleobj2 = ges_track_element_get_nleobject (trackelement2);
  fail_unless (nleobj2 != NULL);
  g_object_get (nleobj2, "priority", &prio2, NULL);
  /* clip2 is on the second layer and has a priority of 1 */
  assert_equals_int (prio2, MIN_NLE_PRIO + LAYER_HEIGHT + 1);

  assert_equals_int (_PRIORITY (clip3), LAYER_HEIGHT - 1);
  nleobj3 = ges_track_element_get_nleobject (trackelement3);
  fail_unless (nleobj3 != NULL);
  /* clip3 is on the third layer and has a priority of LAYER_HEIGHT + 1
   * it priority must have the maximum priority of this layer*/
  g_object_get (nleobj3, "priority", &prio3, NULL);
  assert_equals_int (prio3, MIN_NLE_PRIO + LAYER_HEIGHT * 3 - 1);

  /* Move layers around */
  g_object_set (layer1, "priority", 2, NULL);
  g_object_set (layer2, "priority", 0, NULL);
  g_object_set (layer3, "priority", 1, NULL);
  ges_timeline_commit (timeline);

  /* And check the new priorities */
  assert_equals_int (ges_layer_get_priority (layer1), 2);
  assert_equals_int (ges_layer_get_priority (layer2), 0);
  assert_equals_int (ges_layer_get_priority (layer3), 1);
  assert_equals_int (_PRIORITY (clip1), 0);
  assert_equals_int (_PRIORITY (clip2), 1);
  assert_equals_int (_PRIORITY (clip3), LAYER_HEIGHT - 1);
  g_object_get (nleobj1, "priority", &prio1, NULL);
  g_object_get (nleobj2, "priority", &prio2, NULL);
  g_object_get (nleobj3, "priority", &prio3, NULL);
  assert_equals_int (prio1, 2 * LAYER_HEIGHT + MIN_NLE_PRIO);
  assert_equals_int (prio2, MIN_NLE_PRIO + 1);
  assert_equals_int (prio3, LAYER_HEIGHT * 2 - 1 + MIN_NLE_PRIO);

  /* And move objects around */
  fail_unless (ges_clip_move_to_layer (clip2, layer1));
  fail_unless (ges_clip_move_to_layer (clip3, layer1));
  ges_timeline_commit (timeline);

  objs = ges_layer_get_clips (layer1);
  assert_equals_int (g_list_length (objs), 3);
  fail_unless (ges_layer_get_clips (layer2) == NULL);
  fail_unless (ges_layer_get_clips (layer3) == NULL);

  for (tmp = objs; tmp; tmp = g_list_next (tmp)) {
    gst_object_unref (tmp->data);
  }
  g_list_free (objs);

  /*  Check their priorities (layer1 priority is now 2) */
  assert_equals_int (_PRIORITY (clip1), 0);
  assert_equals_int (_PRIORITY (clip2), 1);
  assert_equals_int (_PRIORITY (clip3), LAYER_HEIGHT - 1);
  g_object_get (nleobj1, "priority", &prio1, NULL);
  g_object_get (nleobj2, "priority", &prio2, NULL);
  g_object_get (nleobj3, "priority", &prio3, NULL);
  assert_equals_int (prio1, 2 * LAYER_HEIGHT + MIN_NLE_PRIO);
  assert_equals_int (prio2, 2 * LAYER_HEIGHT + 1 + MIN_NLE_PRIO);
  assert_equals_int (prio3, LAYER_HEIGHT * 3 - 1 + MIN_NLE_PRIO);

  /* And change TrackElement-s priorities and check that changes are not
   * refected on it containing Clip
   * FIXME : We should rework the way we handle the case were a trackobject
   * prio is set outside the layer it is in.
   * ges_timeline_element_set_priority (GES_TIMELINE_ELEMENT (trackelement3),
   * ges_timeline_commit (timeline);
   *     LAYER_HEIGHT * 2);
   * g_object_get (nleobj3, "priority", &prio3, NULL);
   * assert_equals_int (prio3, 2 * LAYER_HEIGHT);
   * assert_equals_int (_PRIORITY (clip3), LAYER_HEIGHT - 1); */

  gst_object_unref (trackelement1);
  gst_object_unref (trackelement2);
  gst_object_unref (trackelement3);
  gst_object_unref (timeline);
}

GST_END_TEST;

GST_START_TEST (test_timeline_auto_transition)
{
  GESAsset *asset;
  GESTimeline *timeline;
  GESLayer *layer, *layer1, *layer2;

  ges_init ();

  asset = ges_asset_request (GES_TYPE_TEST_CLIP, NULL, NULL);
  fail_unless (GES_IS_ASSET (asset));

  GST_DEBUG ("Create timeline");
  timeline = ges_timeline_new_audio_video ();
  assert_is_type (timeline, GES_TYPE_TIMELINE);

  GST_DEBUG ("Create layers");
  layer = ges_layer_new ();
  assert_is_type (layer, GES_TYPE_LAYER);
  layer1 = ges_layer_new ();
  assert_is_type (layer, GES_TYPE_LAYER);
  layer2 = ges_layer_new ();
  assert_is_type (layer, GES_TYPE_LAYER);

  GST_DEBUG ("Set auto-transition to the layers");
  ges_layer_set_auto_transition (layer, TRUE);
  ges_layer_set_auto_transition (layer1, TRUE);
  ges_layer_set_auto_transition (layer2, TRUE);

  GST_DEBUG ("Add layers to the timeline");
  ges_timeline_add_layer (timeline, layer);
  ges_timeline_add_layer (timeline, layer1);
  ges_timeline_add_layer (timeline, layer2);

  GST_DEBUG ("Check that auto-transition was properly set to the layers");
  fail_unless (ges_layer_get_auto_transition (layer));
  fail_unless (ges_layer_get_auto_transition (layer1));
  fail_unless (ges_layer_get_auto_transition (layer2));

  GST_DEBUG ("Set timeline auto-transition property to FALSE");
  ges_timeline_set_auto_transition (timeline, FALSE);

  GST_DEBUG
      ("Check that layers auto-transition has the same value as timeline");
  fail_if (ges_layer_get_auto_transition (layer));
  fail_if (ges_layer_get_auto_transition (layer1));
  fail_if (ges_layer_get_auto_transition (layer2));

  GST_DEBUG ("Set timeline auto-transition property to TRUE");
  ges_timeline_set_auto_transition (timeline, TRUE);

  GST_DEBUG
      ("Check that layers auto-transition has the same value as timeline");
  fail_unless (ges_layer_get_auto_transition (layer));
  fail_unless (ges_layer_get_auto_transition (layer1));
  fail_unless (ges_layer_get_auto_transition (layer2));

  gst_object_unref (timeline);
}

GST_END_TEST;

GST_START_TEST (test_single_layer_automatic_transition)
{
  GESAsset *asset;
  GESTimeline *timeline;
  GList *objects, *current;
  GESClip *transition;
  GESLayer *layer;
  GESTimelineElement *src, *src1, *src2;

  ges_init ();

  asset = ges_asset_request (GES_TYPE_TEST_CLIP, NULL, NULL);
  fail_unless (GES_IS_ASSET (asset));

  GST_DEBUG ("Create timeline");
  timeline = ges_timeline_new_audio_video ();
  assert_is_type (timeline, GES_TYPE_TIMELINE);

  GST_DEBUG ("Create first layer");
  layer = ges_layer_new ();
  assert_is_type (layer, GES_TYPE_LAYER);

  GST_DEBUG ("Add first layer to timeline");
  fail_unless (ges_timeline_add_layer (timeline, layer));

  GST_DEBUG ("Set auto transition to first layer");
  ges_layer_set_auto_transition (layer, TRUE);

  GST_DEBUG ("Check that auto-transition was properly set");
  fail_unless (ges_layer_get_auto_transition (layer));

  GST_DEBUG ("Adding assets to first layer");
  GST_DEBUG ("Adding clip from 0 -- 1000 to first layer");
  src = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 0, 0,
          1000, GES_TRACK_TYPE_UNKNOWN));
  fail_unless (GES_IS_CLIP (src));

  GST_DEBUG ("Adding clip from 500 -- 1000 to first layer");
  src1 = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 500,
          0, 1000, GES_TRACK_TYPE_UNKNOWN));
  fail_unless (GES_IS_CLIP (src1));

  /*
   *        500__transition__1000
   * 0___________src_________1000
   *        500___________src1_________1500
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  ges_timeline_commit (timeline);

  GST_DEBUG ("Checking that a transition has been added");
  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 4);
  assert_is_type (objects->data, GES_TYPE_TEST_CLIP);

  transition = objects->next->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  transition = objects->next->next->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Moving first source to 250");
  ges_timeline_element_set_start (src, 250);

  /*
   *        500_____transition____1250
   *    250___________src_________1250
   *        500___________src1_________1500
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 250);
  assert_equals_uint64 (_DURATION (src), 1250 - 250);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 4);
  assert_is_type (objects->data, GES_TYPE_TEST_CLIP);

  transition = objects->next->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 750);

  transition = objects->next->next->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_int (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 750);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Moving second source to 250, the transitions should be removed");
  ges_timeline_element_set_start (src1, 250);

  /* The transition should be removed
   *    250___________src_________1250
   *    250___________src1________1250
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 250);
  assert_equals_uint64 (_DURATION (src), 1250 - 250);
  assert_equals_uint64 (_START (src1), 250);
  assert_equals_uint64 (_DURATION (src1), 1250 - 250);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 2);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Trimming second source to 500 no transition should be created "
      "as they have the same end");
  ges_container_edit (GES_CONTAINER (src1), NULL, -1,
      GES_EDIT_MODE_TRIM, GES_EDGE_START, 500);

  /*    250___________src_________1250
   *          500______src1_______1250
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 250);
  assert_equals_uint64 (_DURATION (src), 1250 - 250);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 2);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Trimming second source to 500, no transition should be created");
  ges_timeline_element_trim (src, 500);

  /*        500___________src_________1250
   *        500___________src1________1250
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 500);
  assert_equals_uint64 (_DURATION (src), 1250 - 500);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);

  GST_DEBUG ("Trimming first source to 750, no transition should be created");
  ges_timeline_element_trim (src, 750);

  /*              750_______src_______1250
   *        500___________src1________1250
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 750);
  assert_equals_uint64 (_DURATION (src), 1250 - 750);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 2);
  g_list_free_full (objects, gst_object_unref);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 2);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Moving first source to 500, no transition should be created");
  ges_timeline_element_set_start (src, 500);

  /*        500________src______1000
   *        500___________src1________1250
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 500);
  assert_equals_uint64 (_DURATION (src), 1000 - 500);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 2);
  g_list_free_full (objects, gst_object_unref);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 2);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Moving first source to 600, no transition should be created");
  ges_timeline_element_set_start (src, 600);
  /*             600____src___1100
   *        500___________src1________1250
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 600);
  assert_equals_uint64 (_DURATION (src), 1100 - 600);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 2);
  g_list_free_full (objects, gst_object_unref);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 2);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Adding asset to first layer");
  GST_DEBUG ("Adding clip from 1250 -- 1000 to first layer");
  src2 =
      GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 1250, 0,
          1000, GES_TRACK_TYPE_UNKNOWN));
  assert_is_type (src2, GES_TYPE_TEST_CLIP);

  /*             600____src___1100
   *        500___________src1________1250
   *                                  1250___________src2________2250
   */
  assert_equals_uint64 (_START (src), 600);
  assert_equals_uint64 (_DURATION (src), 1100 - 600);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);
  assert_equals_uint64 (_START (src2), 1250);
  assert_equals_uint64 (_DURATION (src2), 1000);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 3);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG
      ("Changig first source duration to 800 2 transitions should be created");
  ges_timeline_element_set_duration (src, 800);
  ges_timeline_commit (timeline);
  /*             600__________________src_____________1400
   *        500___________src1________1250
   *                                  1250___________src2________2250
   *             600_____trans1_______1250
   *                                  1250___trans2___1400
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 600);
  assert_equals_uint64 (_DURATION (src), 1400 - 600);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);

  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 7);
  assert_is_type (objects->data, GES_TYPE_TEST_CLIP);
  fail_unless (objects->data == src1);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 600);
  assert_equals_uint64 (_DURATION (transition), 1250 - 600);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline + ourself", 3);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 600);
  assert_equals_uint64 (_DURATION (transition), 1250 - 600);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline + ourself", 3);

  current = current->next;
  fail_unless (current->data == src);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1250);
  assert_equals_uint64 (_DURATION (transition), 1400 - 1250);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline + ourself", 3);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1250);
  assert_equals_uint64 (_DURATION (transition), 1400 - 1250);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline + ourself", 3);

  current = current->next;
  fail_unless (current->data == src2);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Back to previous state");
  /*  Make sure to keep 1 ref so we can check_destroyed afterward */
  gst_object_ref (transition);
  ges_timeline_element_set_duration (src, 1100 - 600);
  /*             600____src___1100
   *        500___________src1________1250
   *                                  1250___________src2________2250
   */
  assert_equals_uint64 (_START (src), 600);
  assert_equals_uint64 (_DURATION (src), 1100 - 600);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);
  assert_equals_uint64 (_START (src2), 1250);
  assert_equals_uint64 (_DURATION (src2), 1000);

  /* We check that the transition as actually been freed */
  check_destroyed (G_OBJECT (transition), NULL, NULL);

  objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 3);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG
      ("Set third clip start to 1100, 1 new transition should be created");
  ges_timeline_element_set_start (src2, 1100);
  ges_timeline_commit (timeline);
  /*             600____src___1100
   *        500___________src1________1250
   *                          1100___________src2________2100
   *                          ^__trans___^
   */
  assert_equals_uint64 (_START (src), 600);
  assert_equals_uint64 (_DURATION (src), 1100 - 600);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);
  assert_equals_uint64 (_START (src2), 1100);
  assert_equals_uint64 (_DURATION (src2), 1000);

  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 5);
  assert_is_type (objects->data, GES_TYPE_TEST_CLIP);
  fail_unless (current->data == src1);

  current = current->next;
  fail_unless (current->data == src);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1100);
  assert_equals_uint64 (_DURATION (transition), 1250 - 1100);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1100);
  assert_equals_uint64 (_DURATION (transition), 1250 - 1100);

  current = current->next;
  fail_unless (current->data == src2);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Set third clip start to 1000, Transition should be updated");
  ges_container_edit (GES_CONTAINER (src2), NULL, -1,
      GES_EDIT_MODE_NORMAL, GES_EDGE_START, 1000);
  ges_timeline_commit (timeline);
  /*             600____src___1100
   *                       !_tr__^
   *        500___________src1________1250
   *                       1000___________src2________2000
   *                       ^____trans____^
   */
  assert_equals_uint64 (_START (src), 600);
  assert_equals_uint64 (_DURATION (src), 500);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1250 - 500);
  assert_equals_uint64 (_START (src2), 1000);
  assert_equals_uint64 (_DURATION (src2), 1000);

  current = objects = ges_layer_get_clips (layer);
  current = objects;
  assert_equals_int (g_list_length (objects), 7);
  assert_is_type (objects->data, GES_TYPE_TEST_CLIP);
  fail_unless (current->data == src1);

  current = current->next;
  fail_unless (current->data == src);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 1100 - 1000);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 1100 - 1000);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 1250 - 1000);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 1250 - 1000);

  current = current->next;
  fail_unless (current->data == src2);
  g_list_free_full (objects, gst_object_unref);

  gst_object_unref (timeline);
}

GST_END_TEST;

GST_START_TEST (test_multi_layer_automatic_transition)
{
  GESAsset *asset;
  GESTimeline *timeline;
  GList *objects, *current;
  GESClip *transition;
  GESLayer *layer, *layer1;
  GESTimelineElement *src, *src1, *src2, *src3;

  ges_init ();

  asset = ges_asset_request (GES_TYPE_TEST_CLIP, NULL, NULL);
  fail_unless (GES_IS_ASSET (asset));

  GST_DEBUG ("Create timeline");
  timeline = ges_timeline_new_audio_video ();
  assert_is_type (timeline, GES_TYPE_TIMELINE);

  GST_DEBUG ("Create first layer");
  layer = ges_layer_new ();
  assert_is_type (layer, GES_TYPE_LAYER);

  GST_DEBUG ("Add first layer to timeline");
  fail_unless (ges_timeline_add_layer (timeline, layer));

  GST_DEBUG ("Append a new layer to the timeline");
  layer1 = ges_timeline_append_layer (timeline);
  assert_is_type (layer1, GES_TYPE_LAYER);

  GST_DEBUG ("Set auto transition to first layer");
  ges_layer_set_auto_transition (layer, TRUE);

  GST_DEBUG ("Check that auto-transition was properly set");
  fail_unless (ges_layer_get_auto_transition (layer));
  fail_if (ges_layer_get_auto_transition (layer1));

  GST_DEBUG ("Adding assets to first layer");
  GST_DEBUG ("Adding clip from 0 -- 1000 to first layer");
  src = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 0, 0,
          1000, GES_TRACK_TYPE_UNKNOWN));
  fail_unless (GES_IS_CLIP (src));

  GST_DEBUG ("Adding clip from 500 -- 1000 to first layer");
  src1 = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 500,
          0, 1000, GES_TRACK_TYPE_UNKNOWN));
  ges_timeline_commit (timeline);
  fail_unless (GES_IS_CLIP (src1));

  /*
   *        500__transition__1000
   * 0___________src_________1000
   *        500___________src1_________1500
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);

  GST_DEBUG ("Checking that a transition has been added");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 4);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Adding clip 2 from 500 -- 1000 to second layer");
  src2 = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer1, asset, 0,
          0, 1000, GES_TRACK_TYPE_UNKNOWN));
  GST_DEBUG ("Adding clip 3 from 500 -- 1000 to second layer");
  src3 = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer1, asset, 500,
          0, 1000, GES_TRACK_TYPE_UNKNOWN));
  assert_is_type (src3, GES_TYPE_TEST_CLIP);

  /*        500__transition__1000
   * 0___________src_________1000
   *        500___________src1_________1500
   *----------------------------------------------------
   * 0___________src2_________1000
   *        500___________src3_________1500         Layer1
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 0);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 500);
  assert_equals_uint64 (_DURATION (src3), 1500 - 500);

  GST_DEBUG ("Checking transitions on first layer");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 4);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Checking transitions on second layer");
  current = objects = ges_layer_get_clips (layer1);
  assert_equals_int (g_list_length (objects), 2);
  fail_unless (current->data == src2);
  fail_unless (current->next->data == src3);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG
      ("Set auto transition to second layer, a new transition should be added");
  ges_layer_set_auto_transition (layer1, TRUE);

  /*        500__transition__1000
   * 0___________src_________1000
   *        500___________src1_________1500
   *----------------------------------------------------
   *        500__transition__1000
   * 0__________src2_________1000
   *        500___________src3_________1500         Layer1
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 0);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 500);
  assert_equals_uint64 (_DURATION (src3), 1500 - 500);

  GST_DEBUG ("Checking transitions on first layer");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 4);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Checking transitions has been added on second layer");
  current = objects = ges_layer_get_clips (layer1);
  assert_equals_int (g_list_length (objects), 4);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Moving src3 to 1000. should remove transition");
  ges_timeline_element_set_start (src3, 1000);

  /*        500__transition__1000
   * 0___________src_________1000
   *        500___________src1_________1500                           Layer
   *----------------------------------------------------
   * 0__________src2_________1000
   *                         1000___________src3_________2000         Layer1
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 0);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 1000);
  assert_equals_uint64 (_DURATION (src3), 2000 - 1000);

  GST_DEBUG ("Checking transitions on first layer");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 4);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Checking transitions has been removed on second layer");
  current = objects = ges_layer_get_clips (layer1);
  assert_equals_int (g_list_length (objects), 2);
  fail_unless (current->data == src2);
  fail_unless (current->next->data == src3);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Moving src3 to first layer, should add a transition");
  ges_clip_move_to_layer (GES_CLIP (src3), layer);

  /*        500__transition__1000
   * 0___________src_________1000
   *        500___________src1_________1500
   *                         1000___________src3_________2000   Layer
   *                         1000__tr__1500
   *----------------------------------------------------
   * 0__________src2_________1000                               Layer1
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 0);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 1000);
  assert_equals_uint64 (_DURATION (src3), 2000 - 1000);

  GST_DEBUG ("Checking transitions on first layer");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 7);
  fail_unless (current->data == src);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  fail_unless (current->data == src1);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 1500 - 1000);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 1500 - 1000);

  current = current->next;
  fail_unless (current->data == src3);

  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Checking second layer");
  current = objects = ges_layer_get_clips (layer1);
  assert_equals_int (g_list_length (objects), 1);
  fail_unless (current->data == src2);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG
      ("Moving src to second layer, should remove first transition on first layer");
  ges_clip_move_to_layer (GES_CLIP (src), layer1);

  /*        500___________src1_________1500
   *                         1000___________src3_________2000   Layer
   *                         1000__tr__1500
   *----------------------------------------------------
   * 0___________src_________1000
   * 0__________src2_________1000                               Layer1
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 0);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 1000);
  assert_equals_uint64 (_DURATION (src3), 2000 - 1000);

  GST_DEBUG ("Checking transitions on first layer");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 4);
  fail_unless (current->data == src1);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  fail_unless (current->data == src3);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Checking second layer");
  current = objects = ges_layer_get_clips (layer1);
  assert_equals_int (g_list_length (objects), 2);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);
  assert_is_type (current->next->data, GES_TYPE_TEST_CLIP);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Edit src to first layer start=1500");
  ges_container_edit (GES_CONTAINER (src), NULL, 0,
      GES_EDIT_MODE_NORMAL, GES_EDGE_NONE, 1500);
  /*                                   1500___________src_________2500
   *                                   1500______tr______2000
   *        500___________src1_________1500                 ^
   *                         1000_________^_src3_________2000   Layer
   *                         1000__tr__1500
   *---------------------------------------------------------------------------
   * 0__________src2_________1000                               Layer1
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 1500);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 0);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 1000);
  assert_equals_uint64 (_DURATION (src3), 2000 - 1000);

  GST_DEBUG ("Checking transitions on first layer");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 7);
  fail_unless (current->data == src1);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  fail_unless (current->data == src3);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  fail_unless (current->data == src);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Checking second layer");
  current = objects = ges_layer_get_clips (layer1);
  assert_equals_int (g_list_length (objects), 1);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Ripple src1 to 700");
  ges_container_edit (GES_CONTAINER (src1), NULL, 0,
      GES_EDIT_MODE_RIPPLE, GES_EDGE_NONE, 700);
  /*                                           1700___________src_________2700
   *                                           1700__tr__2000
   *                700___________src1_________1700
   *                         1000___________src3_________2000   Layer
   *                         1000______tr______1700
   *---------------------------------------------------------------------------
   * 0__________src2_________1000                               Layer1
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 1700);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 700);
  assert_equals_uint64 (_DURATION (src1), 1700 - 700);
  assert_equals_uint64 (_START (src2), 0);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 1000);
  assert_equals_uint64 (_DURATION (src3), 2000 - 1000);

  GST_DEBUG ("Checking transitions on first layer");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 7);
  fail_unless (current->data == src1);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 1700 - 1000);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 1700 - 1000);

  current = current->next;
  fail_unless (current->data == src3);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1700);
  assert_equals_uint64 (_DURATION (transition), 2000 - 1700);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1700);
  assert_equals_uint64 (_DURATION (transition), 2000 - 1700);

  current = current->next;
  fail_unless (current->data == src);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Checking second layer");
  current = objects = ges_layer_get_clips (layer1);
  assert_equals_int (g_list_length (objects), 1);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  gst_object_unref (timeline);
}

GST_END_TEST;

GST_START_TEST (test_layer_activate_automatic_transition)
{
  GESAsset *asset, *transition_asset;
  GESTimeline *timeline;
  GESLayer *layer;
  GList *objects, *current;
  GESClip *transition;
  GESTimelineElement *src, *src1, *src2, *src3;

  ges_init ();

  asset = ges_asset_request (GES_TYPE_TEST_CLIP, NULL, NULL);
  transition_asset =
      ges_asset_request (GES_TYPE_TRANSITION_CLIP, "crossfade", NULL);
  fail_unless (GES_IS_ASSET (asset));

  GST_DEBUG ("Create timeline");
  timeline = ges_timeline_new_audio_video ();
  assert_is_type (timeline, GES_TYPE_TIMELINE);

  GST_DEBUG ("Append a layer to the timeline");
  layer = ges_timeline_append_layer (timeline);
  assert_is_type (layer, GES_TYPE_LAYER);

  GST_DEBUG ("Adding clip from 0 -- 1000 to layer");
  src = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 0, 0,
          1000, GES_TRACK_TYPE_UNKNOWN));
  fail_unless (GES_IS_CLIP (src));

  GST_DEBUG ("Adding clip from 500 -- 1000 to first layer");
  src1 = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 500,
          0, 1000, GES_TRACK_TYPE_UNKNOWN));
  fail_unless (GES_IS_CLIP (src1));

  GST_DEBUG ("Adding clip from 1000 -- 2000 to layer");
  src2 = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 1000,
          0, 1000, GES_TRACK_TYPE_UNKNOWN));
  fail_unless (GES_IS_CLIP (src2));

  GST_DEBUG ("Adding clip from 2000 -- 2500 to layer");
  src3 = GES_TIMELINE_ELEMENT (ges_layer_add_asset (layer, asset, 2000,
          0, 500, GES_TRACK_TYPE_UNKNOWN));
  fail_unless (GES_IS_CLIP (src3));

  /*
   * 0___________src_________1000
   *        500___________src1_________1500
   *                         1000____src2_______2000
   *                                            2000_______src2_____2500
   */
  GST_DEBUG ("Checking src timing values");
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 1000);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 2000);
  assert_equals_uint64 (_DURATION (src3), 500);

  GST_DEBUG ("Checking that no transition has been added");
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 4);
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);
  g_list_free_full (objects, gst_object_unref);

  GST_DEBUG ("Adding transition from 1000 -- 1500 to layer");
  transition =
      GES_CLIP (ges_layer_add_asset (layer,
          transition_asset, 1000, 0, 500, GES_TRACK_TYPE_VIDEO));
  fail_unless (GES_IS_TRANSITION_CLIP (transition));
  objects = GES_CONTAINER_CHILDREN (transition);
  assert_equals_int (g_list_length (objects), 1);

  GST_DEBUG ("Checking the transitions");
  /*
   * 0___________src_________1000
   *        500___________src1_________1500
   *                         1000__tr__1500 (1 of the 2 tracks only)
   *                         1000____src2_______2000
   *                                            2000_______src3_____2500
   */
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 5);
  current = current->next;
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);
  current = current->next;
  assert_is_type (current->data, GES_TYPE_TRANSITION_CLIP);
  current = current->next;
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);
  current = current->next;
  assert_is_type (current->data, GES_TYPE_TEST_CLIP);
  g_list_free_full (objects, gst_object_unref);

  ges_layer_set_auto_transition (layer, TRUE);
  /*
   * 0___________src_________1000
   *        500______tr______1000
   *        500___________src1_________1500
   *                         1000__tr__1500
   *                         1000____src2_______2000
   *                                            2000_______src3_____2500
   */
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 8);
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 1000);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 2000);
  assert_equals_uint64 (_DURATION (src3), 500);

  GST_DEBUG ("Checking transitions");
  fail_unless (current->data == src);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  fail_unless (current->data == src1);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1000);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  fail_unless (current->data == src2);

  current = current->next;
  fail_unless (current->data == src3);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);

  GST_DEBUG ("Moving src2 to 1200, check everything updates properly");
  ges_timeline_element_set_start (src2, 1200);
  ges_timeline_commit (timeline);
  /*
   * 0___________src_________1000
   *        500______tr______1000
   *        500___________src1_________1500
   *                           1200_tr_1500
   *                           1200____src2_______2200
   *                                          !__tr__^
   *                                          2000_______src3_____2500
   */
  current = objects = ges_layer_get_clips (layer);
  assert_equals_int (g_list_length (objects), 10);
  assert_equals_uint64 (_START (src), 0);
  assert_equals_uint64 (_DURATION (src), 1000);
  assert_equals_uint64 (_START (src1), 500);
  assert_equals_uint64 (_DURATION (src1), 1500 - 500);
  assert_equals_uint64 (_START (src2), 1200);
  assert_equals_uint64 (_DURATION (src2), 1000);
  assert_equals_uint64 (_START (src3), 2000);
  assert_equals_uint64 (_DURATION (src3), 500);

  GST_DEBUG ("Checking transitions");
  fail_unless (current->data == src);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 500);
  assert_equals_uint64 (_DURATION (transition), 500);

  current = current->next;
  fail_unless (current->data == src1);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1200);
  assert_equals_uint64 (_DURATION (transition), 300);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 1200);
  assert_equals_uint64 (_DURATION (transition), 300);

  current = current->next;
  fail_unless (current->data == src2);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 2000);
  assert_equals_uint64 (_DURATION (transition), 200);

  current = current->next;
  transition = current->data;
  assert_is_type (transition, GES_TYPE_TRANSITION_CLIP);
  assert_equals_uint64 (_START (transition), 2000);
  assert_equals_uint64 (_DURATION (transition), 200);

  current = current->next;
  fail_unless (current->data == src3);
  g_list_free_full (objects, gst_object_unref);
  ASSERT_OBJECT_REFCOUNT (transition, "layer + timeline", 2);


  gst_object_unref (timeline);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_string)
{
  GESTimeline *timeline;
  GESLayer *layer;
  const gchar *result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  ges_meta_container_set_string (GES_META_CONTAINER (layer),
      "ges-test", "blub");

  fail_unless ((result = ges_meta_container_get_string (GES_META_CONTAINER
              (layer), "ges-test")) != NULL);

  assert_equals_string (result, "blub");
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_boolean)
{
  GESTimeline *timeline;
  GESLayer *layer;
  gboolean result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  ges_meta_container_set_boolean (GES_META_CONTAINER (layer), "ges-test", TRUE);

  fail_unless (ges_meta_container_get_boolean (GES_META_CONTAINER
          (layer), "ges-test", &result));

  fail_unless (result);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_int)
{
  GESTimeline *timeline;
  GESLayer *layer;
  gint result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  ges_meta_container_set_int (GES_META_CONTAINER (layer), "ges-test", 1234);

  fail_unless (ges_meta_container_get_int (GES_META_CONTAINER (layer),
          "ges-test", &result));

  assert_equals_int (result, 1234);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_uint)
{
  GESTimeline *timeline;
  GESLayer *layer;
  guint result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  ges_meta_container_set_uint (GES_META_CONTAINER (layer), "ges-test", 42);

  fail_unless (ges_meta_container_get_uint (GES_META_CONTAINER
          (layer), "ges-test", &result));

  assert_equals_int (result, 42);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_int64)
{
  GESTimeline *timeline;
  GESLayer *layer;
  gint64 result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  ges_meta_container_set_int64 (GES_META_CONTAINER (layer), "ges-test", 1234);

  fail_unless (ges_meta_container_get_int64 (GES_META_CONTAINER (layer),
          "ges-test", &result));

  assert_equals_int64 (result, 1234);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_uint64)
{
  GESTimeline *timeline;
  GESLayer *layer;
  guint64 result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  ges_meta_container_set_uint64 (GES_META_CONTAINER (layer), "ges-test", 42);

  fail_unless (ges_meta_container_get_uint64 (GES_META_CONTAINER
          (layer), "ges-test", &result));

  assert_equals_uint64 (result, 42);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_float)
{
  GESTimeline *timeline;
  GESLayer *layer;
  gfloat result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  fail_unless (ges_meta_container_set_float (GES_META_CONTAINER (layer),
          "ges-test", 23.456));

  fail_unless (ges_meta_container_get_float (GES_META_CONTAINER (layer),
          "ges-test", &result));

  assert_equals_float (result, 23.456f);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_double)
{
  GESTimeline *timeline;
  GESLayer *layer;
  gdouble result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  ges_meta_container_set_double (GES_META_CONTAINER (layer),
      "ges-test", 23.456);

  fail_unless (ges_meta_container_get_double (GES_META_CONTAINER
          (layer), "ges-test", &result));
  fail_unless (result == 23.456);

  //TODO CHECK
  assert_equals_float (result, 23.456);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_date)
{
  GESTimeline *timeline;
  GESLayer *layer;
  GDate *input;
  GDate *result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  input = g_date_new_dmy (1, 1, 2012);

  ges_meta_container_set_date (GES_META_CONTAINER (layer), "ges-test", input);

  fail_unless (ges_meta_container_get_date (GES_META_CONTAINER
          (layer), "ges-test", &result));

  fail_unless (g_date_compare (result, input) == 0);

  g_date_free (input);
  g_date_free (result);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_date_time)
{
  GESTimeline *timeline;
  GESLayer *layer;
  GstDateTime *input;
  GstDateTime *result = NULL;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  input = gst_date_time_new_from_unix_epoch_local_time (123456789);

  fail_unless (ges_meta_container_set_date_time (GES_META_CONTAINER
          (layer), "ges-test", input));

  fail_unless (ges_meta_container_get_date_time (GES_META_CONTAINER
          (layer), "ges-test", &result));

  fail_unless (result != NULL);
  fail_unless (gst_date_time_get_day (input) == gst_date_time_get_day (result));
  fail_unless (gst_date_time_get_hour (input) ==
      gst_date_time_get_hour (result));

  gst_date_time_unref (input);
  gst_date_time_unref (result);
}

GST_END_TEST;


GST_START_TEST (test_layer_meta_value)
{
  GESTimeline *timeline;
  GESLayer *layer;
  GValue data = G_VALUE_INIT;
  const GValue *result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  g_value_init (&data, G_TYPE_STRING);
  g_value_set_string (&data, "Hello world!");

  ges_meta_container_set_meta (GES_META_CONTAINER (layer),
      "ges-test-value", &data);

  result =
      ges_meta_container_get_meta (GES_META_CONTAINER (layer),
      "ges-test-value");
  assert_equals_string (g_value_get_string (result), "Hello world!");

  g_value_unset (&data);
}

GST_END_TEST;

GST_START_TEST (test_layer_meta_register)
{
  GESTimeline *timeline;
  GESLayer *layer;
  const gchar *result;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  fail_unless (ges_meta_container_register_meta_string (GES_META_CONTAINER
          (layer), GES_META_READABLE, "ges-test-value", "Hello world!"));

  result = ges_meta_container_get_string (GES_META_CONTAINER (layer),
      "ges-test-value");
  assert_equals_string (result, "Hello world!");

  fail_if (ges_meta_container_set_int (GES_META_CONTAINER (layer),
          "ges-test-value", 123456));

  result = ges_meta_container_get_string (GES_META_CONTAINER (layer),
      "ges-test-value");
  assert_equals_string (result, "Hello world!");
}

GST_END_TEST;

static void
test_foreach (const GESMetaContainer * container, const gchar * key,
    GValue * value, gpointer user_data)
{
  fail_unless ((0 == g_strcmp0 (key, "some-string")) ||
      (0 == g_strcmp0 (key, "some-int")) || (0 == g_strcmp0 (key, "volume")));
}

GST_START_TEST (test_layer_meta_foreach)
{
  GESTimeline *timeline;
  GESLayer *layer;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();
  layer = ges_layer_new ();
  ges_timeline_add_layer (timeline, layer);

  ges_meta_container_set_string (GES_META_CONTAINER (layer),
      "some-string", "some-content");

  ges_meta_container_set_int (GES_META_CONTAINER (layer), "some-int", 123456);

  ges_meta_container_foreach (GES_META_CONTAINER (layer),
      (GESMetaForeachFunc) test_foreach, NULL);
}

GST_END_TEST;

static Suite *
ges_suite (void)
{
  Suite *s = suite_create ("ges-layer");
  TCase *tc_chain = tcase_create ("timeline-layer");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_layer_properties);
  tcase_add_test (tc_chain, test_layer_priorities);
  tcase_add_test (tc_chain, test_timeline_auto_transition);
  tcase_add_test (tc_chain, test_single_layer_automatic_transition);
  tcase_add_test (tc_chain, test_multi_layer_automatic_transition);
  tcase_add_test (tc_chain, test_layer_activate_automatic_transition);
  tcase_add_test (tc_chain, test_layer_meta_string);
  tcase_add_test (tc_chain, test_layer_meta_boolean);
  tcase_add_test (tc_chain, test_layer_meta_int);
  tcase_add_test (tc_chain, test_layer_meta_uint);
  tcase_add_test (tc_chain, test_layer_meta_int64);
  tcase_add_test (tc_chain, test_layer_meta_uint64);
  tcase_add_test (tc_chain, test_layer_meta_float);
  tcase_add_test (tc_chain, test_layer_meta_double);
  tcase_add_test (tc_chain, test_layer_meta_date);
  tcase_add_test (tc_chain, test_layer_meta_date_time);
  tcase_add_test (tc_chain, test_layer_meta_value);
  tcase_add_test (tc_chain, test_layer_meta_register);
  tcase_add_test (tc_chain, test_layer_meta_foreach);

  return s;
}

GST_CHECK_MAIN (ges);
