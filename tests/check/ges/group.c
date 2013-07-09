/* GStreamer Editing Services
 *
 * Copyright (C) <2013> Thibault Saunier <thibault.saunier@collabora.com>
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

GST_START_TEST (test_move_group)
{
  GESAsset *asset;
  GESGroup *group;
  GESTimeline *timeline;
  GESLayer *layer, *layer1;
  GESClip *clip, *clip1, *clip2;

  GList *clips = NULL;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();

  layer = ges_timeline_append_layer (timeline);
  layer1 = ges_timeline_append_layer (timeline);
  asset = ges_asset_request (GES_TYPE_TEST_CLIP, NULL, NULL);

  /**
   * Our timeline:
   * -------------
   *
   *          0------------Group1---------------110
   *          |--------                          |
   * layer:   |  clip  |                         |
   *          |-------10                         |
   *          |----------------------------------|
   *          |        0---------    0-----------|
   * layer1:  |        | clip1   |    |  clip2   |
   *          |       10--------20   50----------|
   *          |----------------------------------|
   */
  clip = ges_layer_add_asset (layer, asset, 0, 0, 10, GES_TRACK_TYPE_UNKNOWN);
  clip1 =
      ges_layer_add_asset (layer1, asset, 10, 0, 10, GES_TRACK_TYPE_UNKNOWN);
  clip2 =
      ges_layer_add_asset (layer1, asset, 50, 0, 60, GES_TRACK_TYPE_UNKNOWN);
  clips = g_list_prepend (clips, clip);
  clips = g_list_prepend (clips, clip1);
  clips = g_list_prepend (clips, clip2);
  group = GES_GROUP (ges_container_group (clips));
  g_list_free (clips);

  fail_unless (GES_IS_GROUP (group));
  ASSERT_OBJECT_REFCOUNT (group, "1 ref for the timeline", 1);
  fail_unless (g_list_length (GES_CONTAINER_CHILDREN (group)) == 3);
  assert_equals_int (GES_CONTAINER_HEIGHT (group), 2);
  CHECK_OBJECT_PROPS (clip, 0, 0, 10);
  CHECK_OBJECT_PROPS (clip1, 10, 0, 10);
  CHECK_OBJECT_PROPS (clip2, 50, 0, 60);
  CHECK_OBJECT_PROPS (group, 0, 0, 110);

  /*
   *        0  10------------Group1---------------120
   *            |--------                          |
   * layer:     |  clip  |                         |
   *            |-------20                         |
   *            |----------------------------------|
   *            |        0---------    0-----------|
   * layer1:    |        | clip1   |    |  clip2   |
   *            |       20--------30   60----------|
   *            |----------------------------------|
   */
  ges_timeline_element_set_start (GES_TIMELINE_ELEMENT (clip), 10);
  CHECK_OBJECT_PROPS (clip, 10, 0, 10);
  CHECK_OBJECT_PROPS (clip1, 20, 0, 10);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 60);
  CHECK_OBJECT_PROPS (group, 10, 0, 110);

  /*
   *        0  10------------Group1---------------120
   *            |------                            |
   * layer:     |clip  |                           |
   *            |-----15                           |
   *            |----------------------------------|
   *            |        0---------    0-----------|
   * layer1:    |        | clip1   |    |  clip2   |
   *            |       20--------30   60----------|
   *            |----------------------------------|
   */
  ges_timeline_element_set_duration (GES_TIMELINE_ELEMENT (clip), 5);
  CHECK_OBJECT_PROPS (clip, 10, 0, 5);
  CHECK_OBJECT_PROPS (clip1, 20, 0, 10);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 60);
  CHECK_OBJECT_PROPS (group, 10, 0, 110);

  /*
   *        0  10------------Group1---------------110
   *            |------                            |
   * layer:     |clip  |                           |
   *            |-----15                           |
   *            |----------------------------------|
   *            |        0---------    0-----------|
   * layer1:    |        | clip1   |    |  clip2   |
   *            |       20--------30   60----------|
   *            |----------------------------------|
   */
  ges_timeline_element_set_duration (GES_TIMELINE_ELEMENT (clip2), 50);
  CHECK_OBJECT_PROPS (clip, 10, 0, 5);
  CHECK_OBJECT_PROPS (clip1, 20, 0, 10);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 50);
  CHECK_OBJECT_PROPS (group, 10, 0, 100);

  /*
   *        0  10------------Group1---------------110
   *            |------                            |
   * layer:     |clip  |                           |
   *            |-----15                           |
   *            |----------------------------------|
   *            |        5---------    0-----------|
   * layer1:    |        | clip1   |    |  clip2   |
   *            |       20--------30   60----------|
   *            |----------------------------------|
   */
  ges_timeline_element_set_inpoint (GES_TIMELINE_ELEMENT (clip1), 5);
  CHECK_OBJECT_PROPS (clip, 10, 0, 5);
  CHECK_OBJECT_PROPS (clip1, 20, 5, 10);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 50);
  CHECK_OBJECT_PROPS (group, 10, 0, 100);

  /*
   *        0  10------------Group1---------------110
   *            |------                            |
   * layer:     |clip  |                           |
   *            |-----15                           |
   *            |----------------------------------|
   *            |        5---------    0-----------|
   * layer1:    |        | clip1   |    |  clip2   |
   *            |       20--------30   60----------|
   *            |----------------------------------|
   */
  ges_timeline_element_set_inpoint (GES_TIMELINE_ELEMENT (clip1), 5);
  CHECK_OBJECT_PROPS (clip, 10, 0, 5);
  CHECK_OBJECT_PROPS (clip1, 20, 5, 10);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 50);
  CHECK_OBJECT_PROPS (group, 10, 0, 100);

  /*
   *        0           20---Group1---------------110
   *                    |                          |
   * layer:             |                          |
   *                    |                          |
   *                    |--------------------------|
   *                    5---------    0------------|
   * layer1:            | clip1   |    |  clip2    |
   *                    20--------30   60----------|
   *                    |--------------------------|
   */
  ges_timeline_element_trim (GES_TIMELINE_ELEMENT (group), 20);
  CHECK_OBJECT_PROPS (clip, 15, 5, 0);
  CHECK_OBJECT_PROPS (clip1, 20, 5, 10);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 50);
  CHECK_OBJECT_PROPS (group, 20, 0, 90);

  /*
   *        0             25---Group1---------------110
   *                       |                          |
   * layer:                |                          |
   *                       |                          |
   *                       |--------------------------|
   *                       10------      0------------|
   * layer1:               | clip1 |      |  clip2    |
   *                      25------30      60----------|
   *                       |--------------------------|
   */
  ges_timeline_element_trim (GES_TIMELINE_ELEMENT (group), 25);
  CHECK_OBJECT_PROPS (clip, 15, 5, 0);
  CHECK_OBJECT_PROPS (clip1, 25, 10, 5);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 50);
  CHECK_OBJECT_PROPS (group, 25, 0, 85);

  /*
   *        0  10------------Group1---------------110
   *            |------                            |
   * layer:     |clip  |                           |
   *            |-----15                           |
   *            |----------------------------------|
   *            0-----------------     0-----------|
   * layer1:    |    clip1        |    |   clip2   |
   *            |-----------------30   60----------|
   *            |----------------------------------|
   */
  ges_timeline_element_trim (GES_TIMELINE_ELEMENT (group), 10);
  CHECK_OBJECT_PROPS (clip, 10, 0, 5);
  CHECK_OBJECT_PROPS (clip1, 10, 0, 20);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 50);
  CHECK_OBJECT_PROPS (group, 10, 0, 100);

  /*
   *        0             25---Group1---------------110
   *                       |                          |
   * layer:         15     |                          |
   *                 |clip |                          |
   *                 -     |--------------------------|
   *                       15------      0------------|
   * layer1:               | clip1 |      |  clip2    |
   *                      25------30      60----------|
   *                       |--------------------------|
   */
  ges_timeline_element_trim (GES_TIMELINE_ELEMENT (group), 25);
  CHECK_OBJECT_PROPS (clip, 15, 5, 0);
  CHECK_OBJECT_PROPS (clip1, 25, 15, 5);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 50);
  CHECK_OBJECT_PROPS (group, 25, 0, 85);

  /*
   *        0             25---Group1--30
   *                       |            |
   * layer:          15    |            |
   *                 |clip |            |
   *                  -    |------------
   *                       15-----------|   60
   * layer1:               | clip1      |   |clip2
   *                      25------------|   -
   *                       |------------|
   */
  ges_timeline_element_set_duration (GES_TIMELINE_ELEMENT (group), 10);
  CHECK_OBJECT_PROPS (clip, 15, 5, 0);
  CHECK_OBJECT_PROPS (clip1, 25, 15, 5);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 0);
  CHECK_OBJECT_PROPS (group, 25, 0, 5);

  /*
   *        0             25---Group1---------------125
   *                       |                          |
   * layer:        15      |                          |
   *                |clip  |                          |
   *                -      |--------------------------|
   *                       15-------------------------|
   * layer1:               |  clip1       |  clip2    |
   *                      25--------------60----------|
   *                       |--------------------------|
   */
  ges_timeline_element_set_duration (GES_TIMELINE_ELEMENT (group), 100);
  CHECK_OBJECT_PROPS (clip, 15, 5, 0);
  CHECK_OBJECT_PROPS (clip1, 25, 15, 100);
  CHECK_OBJECT_PROPS (clip2, 60, 0, 65);
  CHECK_OBJECT_PROPS (group, 25, 0, 100);

  /*
   *        0           20---Group1---------------120
   *                    |                          |
   * layer:        15   |                          |
   *               |clip|                          |
   *               -    |--------------------------|
   *                    15-------------------------|
   * layer1:            |  clip1       |  clip2    |
   *                    20-------------55----------|
   *                    |--------------------------|
   */
  ges_timeline_element_set_start (GES_TIMELINE_ELEMENT (group), 20);
  CHECK_OBJECT_PROPS (clip, 15, 5, 0);
  CHECK_OBJECT_PROPS (clip1, 20, 15, 100);
  CHECK_OBJECT_PROPS (clip2, 55, 0, 65);
  CHECK_OBJECT_PROPS (group, 20, 0, 100);

  /*
   *        0      10---Group1---------------120
   *               |-----15                   |
   * layer:        | clip|                    |
   *               |------                    |
   *               |--------------------------|
   *               5--------------------------|
   * layer1:       |  clip1       |  clip2    |
   *               10-------------55----------|
   *               |--------------------------|
   */
  ges_timeline_element_trim (GES_TIMELINE_ELEMENT (group), 10);
  CHECK_OBJECT_PROPS (clip, 10, 0, 5);
  CHECK_OBJECT_PROPS (clip1, 10, 5, 110);
  CHECK_OBJECT_PROPS (clip2, 55, 0, 65);
  CHECK_OBJECT_PROPS (group, 10, 0, 110);

  ASSERT_OBJECT_REFCOUNT (group, "1 ref for the timeline", 1);
  check_destroyed (G_OBJECT (timeline), G_OBJECT (group), NULL);
  gst_object_unref (asset);
}

GST_END_TEST;



static void
_changed_layer_cb (GESTimelineElement * clip,
    GParamSpec * arg G_GNUC_UNUSED, guint * nb_calls)
{
  *nb_calls = *nb_calls + 1;
}

GST_START_TEST (test_group_in_group)
{
  GESAsset *asset;
  GESTimeline *timeline;
  GESGroup *group, *group1;
  GESLayer *layer, *layer1, *layer2, *layer3;
  GESClip *c, *c1, *c2, *c3, *c4, *c5;

  guint nb_layer_notifies = 0;
  GList *clips = NULL;

  ges_init ();

  timeline = ges_timeline_new_audio_video ();

  /* Our timeline
   *
   *    --0------------10-Group-----20---------------30-------Group1----------70
   *      | +-----------+                             |+-----------50         |
   * L    | |    C      |                             ||     C3    |          |
   *      | +-----------+                             |+-----------+          |
   *    --|-------------------------------------------|-----40----------------|
   *      |            +------------+ +-------------+ |      +--------60      |
   * L1   |            |     C1     | |     C2      | |      |     C4 |       |
   *      |            +------------+ +-------------+ |      +--------+       |
   *    --|-------------------------------------------|-----------------------|
   *      |                                           |             +--------+|
   * L2   |                                           |             |  c5    ||
   *      |                                           |             +--------+|
   *    --+-------------------------------------------+-----------------------+
   *
   * L3
   *
   *    -----------------------------------------------------------------------
   */

  layer = ges_timeline_append_layer (timeline);
  layer1 = ges_timeline_append_layer (timeline);
  layer2 = ges_timeline_append_layer (timeline);
  layer3 = ges_timeline_append_layer (timeline);
  assert_equals_int (ges_layer_get_priority (layer3), 3);
  asset = ges_asset_request (GES_TYPE_TEST_CLIP, NULL, NULL);

  c = ges_layer_add_asset (layer, asset, 0, 0, 10, GES_TRACK_TYPE_UNKNOWN);
  c1 = ges_layer_add_asset (layer1, asset, 10, 0, 10, GES_TRACK_TYPE_UNKNOWN);
  c2 = ges_layer_add_asset (layer1, asset, 20, 0, 10, GES_TRACK_TYPE_UNKNOWN);
  clips = g_list_prepend (clips, c);
  clips = g_list_prepend (clips, c1);
  clips = g_list_prepend (clips, c2);
  group = GES_GROUP (ges_container_group (clips));
  fail_unless (GES_TIMELINE_ELEMENT_TIMELINE (group) == timeline);
  g_list_free (clips);

  fail_unless (GES_IS_GROUP (group));
  CHECK_OBJECT_PROPS (c, 0, 0, 10);
  CHECK_OBJECT_PROPS (c1, 10, 0, 10);
  CHECK_OBJECT_PROPS (c2, 20, 0, 10);
  CHECK_OBJECT_PROPS (group, 0, 0, 30);

  c3 = ges_layer_add_asset (layer, asset, 30, 0, 20, GES_TRACK_TYPE_UNKNOWN);
  c4 = ges_layer_add_asset (layer1, asset, 40, 0, 20, GES_TRACK_TYPE_UNKNOWN);
  c5 = ges_layer_add_asset (layer2, asset, 50, 0, 20, GES_TRACK_TYPE_UNKNOWN);
  clips = g_list_prepend (NULL, c3);
  clips = g_list_prepend (clips, c4);
  clips = g_list_prepend (clips, c5);
  group1 = GES_GROUP (ges_container_group (clips));
  fail_unless (GES_TIMELINE_ELEMENT_TIMELINE (group1) == timeline);
  g_list_free (clips);

  fail_unless (GES_IS_GROUP (group1));
  CHECK_OBJECT_PROPS (c3, 30, 0, 20);
  CHECK_OBJECT_PROPS (c4, 40, 0, 20);
  CHECK_OBJECT_PROPS (c5, 50, 0, 20);
  CHECK_OBJECT_PROPS (group1, 30, 0, 40);
  check_layer (c, 0);
  check_layer (c1, 1);
  check_layer (c2, 1);
  check_layer (c3, 0);
  check_layer (c4, 1);
  check_layer (c5, 2);

  fail_unless (ges_container_add (GES_CONTAINER (group),
          GES_TIMELINE_ELEMENT (group1)));
  CHECK_OBJECT_PROPS (c, 0, 0, 10);
  CHECK_OBJECT_PROPS (c1, 10, 0, 10);
  CHECK_OBJECT_PROPS (c2, 20, 0, 10);
  CHECK_OBJECT_PROPS (c3, 30, 0, 20);
  CHECK_OBJECT_PROPS (c4, 40, 0, 20);
  CHECK_OBJECT_PROPS (c5, 50, 0, 20);
  CHECK_OBJECT_PROPS (group, 0, 0, 70);
  CHECK_OBJECT_PROPS (group1, 30, 0, 40);
  check_layer (c, 0);
  check_layer (c1, 1);
  check_layer (c2, 1);
  check_layer (c3, 0);
  check_layer (c4, 1);
  check_layer (c5, 2);

  fail_unless (GES_TIMELINE_ELEMENT_TIMELINE (group) == timeline);
  fail_unless (GES_TIMELINE_ELEMENT_TIMELINE (group1) == timeline);

  ges_timeline_element_set_start (GES_TIMELINE_ELEMENT (c4), 50);
  CHECK_OBJECT_PROPS (c, 10, 0, 10);
  CHECK_OBJECT_PROPS (c1, 20, 0, 10);
  CHECK_OBJECT_PROPS (c2, 30, 0, 10);
  CHECK_OBJECT_PROPS (c3, 40, 0, 20);
  CHECK_OBJECT_PROPS (c4, 50, 0, 20);
  CHECK_OBJECT_PROPS (c5, 60, 0, 20);
  CHECK_OBJECT_PROPS (group, 10, 0, 70);
  CHECK_OBJECT_PROPS (group1, 40, 0, 40);
  fail_unless (GES_TIMELINE_ELEMENT_TIMELINE (group) == timeline);
  fail_unless (GES_TIMELINE_ELEMENT_TIMELINE (group1) == timeline);
  check_layer (c, 0);
  check_layer (c1, 1);
  check_layer (c2, 1);
  check_layer (c3, 0);
  check_layer (c4, 1);
  check_layer (c5, 2);

  /* Our timeline
   *
   * L
   *    -----------------------------------------------------------------------
   *      0------------10-Group-----20---------------30-------Group1----------70
   *      | +-----------+                             |+-----------50         |
   * L1   | |    C      |                             ||     C3    |          |
   *      | +-----------+                             |+-----------+          |
   *      |                                           |                       |
   *    --|-------------------------------------------|-----40----------------|
   *      |            +------------+ +-------------+ |      +--------60      |
   * L2   |            |     C1     | |     C2      | |      |     C4 |       |
   *      |            +------------+ +-------------+ |      +--------+       |
   *    --|-------------------------------------------|-----------------------|
   *      |                                           |             +--------+|
   * L3   |                                           |             |  c5    ||
   *      |                                           |             +--------+|
   *    --+-------------------------------------------+-----------------------+
   */
  fail_unless (ges_clip_move_to_layer (c, layer1));
  check_layer (c, 1);
  check_layer (c1, 2);
  check_layer (c2, 2);
  check_layer (c3, 1);
  check_layer (c4, 2);
  check_layer (c5, 3);
  assert_equals_int (_PRIORITY (group), 1);
  assert_equals_int (_PRIORITY (group1), 1);

  /* We can not move so far! */
  g_signal_connect_after (c4, "notify::layer",
      (GCallback) _changed_layer_cb, &nb_layer_notifies);
  fail_if (ges_clip_move_to_layer (c4, layer));
  assert_equals_int (nb_layer_notifies, 0);
  check_layer (c, 1);
  check_layer (c1, 2);
  check_layer (c2, 2);
  check_layer (c3, 1);
  check_layer (c4, 2);
  check_layer (c5, 3);
  assert_equals_int (_PRIORITY (group), 1);
  assert_equals_int (_PRIORITY (group1), 1);

  clips = ges_container_ungroup (GES_CONTAINER (group), FALSE);
  assert_equals_int (g_list_length (clips), 4);
  g_list_free_full (clips, gst_object_unref);

  gst_object_unref (timeline);
  gst_object_unref (asset);
}

GST_END_TEST;

static Suite *
ges_suite (void)
{
  Suite *s = suite_create ("ges-group");
  TCase *tc_chain = tcase_create ("group");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_move_group);
  tcase_add_test (tc_chain, test_group_in_group);

  return s;
}

GST_CHECK_MAIN (ges);
