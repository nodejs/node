#!/usr/bin/env python3

# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import contextlib
import io
import os
import unittest

from tempfile import TemporaryDirectory
from unittest.mock import patch

from download_profiles import main


class TestDownloadProfiles(unittest.TestCase):

  def _test_cmd(self, cmd, exitcode):
    out = io.StringIO()
    err = io.StringIO()
    with self.assertRaises(SystemExit) as se, \
        contextlib.redirect_stdout(out), \
        contextlib.redirect_stderr(err):
      main(cmd)
    self.assertEqual(se.exception.code, exitcode)
    return out.getvalue(), err.getvalue()

  def test_validate_profiles(self):
    out, err = self._test_cmd(['validate', '--version', '11.1.0.0'], 0)
    self.assertEqual(len(out), 0)
    self.assertEqual(len(err), 0)

  def test_download_profiles(self):
    with TemporaryDirectory() as td, \
        patch('download_profiles.PGO_PROFILE_DIR', td):
      out, err = self._test_cmd(['download', '--version', '11.1.0.0'], 0)
      self.assertEqual(len(out), 0)
      self.assertEqual(len(err), 0)
      self.assertGreater(
          len([f for f in os.listdir(td) if f.endswith('.profile')]), 0)

  def test_invalid_args(self):
    out, err = self._test_cmd(['invalid-action'], 2)
    self.assertEqual(len(out), 0)
    self.assertGreater(len(err), 0)

  def test_invalid_depot_tools_path(self):
    out, err = self._test_cmd(
        ['validate', '--depot-tools', '/no-depot-tools-path'], 3)
    self.assertEqual(len(out), 0)
    self.assertGreater(len(err), 0)

  def test_missing_profiles(self):
    out, err = self._test_cmd(['download', '--version', '0.0.0.42'], 4)
    self.assertEqual(len(out), 0)
    self.assertGreater(len(err), 0)


if __name__ == '__main__':
  unittest.main()
