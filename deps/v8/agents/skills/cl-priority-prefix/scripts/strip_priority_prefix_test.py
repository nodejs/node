#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

import strip_priority_prefix


class StripPriorityPrefixTest(unittest.TestCase):

  def test_strip_priority_from_message_no_space(self):
    msg = "[P1][compiler] Fix bug\n\nBody text.\n\nChange-Id: I123\n"
    stripped, new_msg, old_title, new_title = (
        strip_priority_prefix.strip_priority_from_message(msg))
    self.assertTrue(stripped)
    self.assertEqual(old_title, "[P1][compiler] Fix bug")
    self.assertEqual(new_title, "[compiler] Fix bug")
    self.assertEqual(new_msg,
                     "[compiler] Fix bug\n\nBody text.\n\nChange-Id: I123\n")

  def test_strip_priority_from_message_with_space(self):
    msg = "[P2] [tools] Update presubmit\n\nBody.\n"
    stripped, new_msg, old_title, new_title = (
        strip_priority_prefix.strip_priority_from_message(msg))
    self.assertTrue(stripped)
    self.assertEqual(old_title, "[P2] [tools] Update presubmit")
    self.assertEqual(new_title, "[tools] Update presubmit")
    self.assertEqual(new_msg, "[tools] Update presubmit\n\nBody.\n")

  def test_strip_priority_already_stripped_is_noop(self):
    msg = "[compiler] Fix bug\n\nBody.\n"
    stripped, new_msg, old_title, new_title = (
        strip_priority_prefix.strip_priority_from_message(msg))
    self.assertFalse(stripped)
    self.assertEqual(new_msg, msg)
    self.assertEqual(old_title, "[compiler] Fix bug")
    self.assertEqual(new_title, "[compiler] Fix bug")

  @mock.patch.object(strip_priority_prefix, "update_gerrit_description")
  @mock.patch.object(strip_priority_prefix, "amend_local_commit")
  @mock.patch.object(
      strip_priority_prefix,
      "get_gerrit_description",
      return_value="[P1][maglev] Fix phis\n\nBody.\n\nChange-Id: Iabc\n")
  @mock.patch.object(
      strip_priority_prefix, "has_gerrit_issue", return_value=True)
  def test_main_syncs_local_and_gerrit(self, mock_has_issue, mock_get_desc,
                                       mock_amend, mock_update_gerrit):
    rc = strip_priority_prefix.main([])
    self.assertEqual(rc, 0)
    expected = "[maglev] Fix phis\n\nBody.\n\nChange-Id: Iabc\n"
    mock_amend.assert_called_once_with(expected, cwd=None)
    mock_update_gerrit.assert_called_once_with(expected, cwd=None)

  @mock.patch.object(strip_priority_prefix, "update_gerrit_description")
  @mock.patch.object(strip_priority_prefix, "amend_local_commit")
  @mock.patch.object(
      strip_priority_prefix,
      "get_local_commit_message",
      return_value="[P3] [d8] Add flag\n\nBody.\n")
  def test_main_local_only(self, mock_get_local, mock_amend,
                           mock_update_gerrit):
    rc = strip_priority_prefix.main(["--local-only"])
    self.assertEqual(rc, 0)
    mock_amend.assert_called_once_with("[d8] Add flag\n\nBody.\n", cwd=None)
    mock_update_gerrit.assert_not_called()

  @mock.patch.object(strip_priority_prefix, "update_gerrit_description")
  @mock.patch.object(strip_priority_prefix, "amend_local_commit")
  @mock.patch.object(strip_priority_prefix, "get_local_commit_message")
  @mock.patch.object(
      strip_priority_prefix,
      "get_gerrit_description",
      side_effect=RuntimeError("Failed to read Gerrit CL description: network"))
  @mock.patch.object(
      strip_priority_prefix, "has_gerrit_issue", return_value=True)
  def test_main_aborts_if_gerrit_description_fails(self, mock_has_issue,
                                                   mock_get_desc,
                                                   mock_get_local, mock_amend,
                                                   mock_update_gerrit):
    rc = strip_priority_prefix.main([])
    self.assertEqual(rc, 1)
    mock_get_local.assert_not_called()
    mock_amend.assert_not_called()
    mock_update_gerrit.assert_not_called()


if __name__ == "__main__":
  unittest.main()
