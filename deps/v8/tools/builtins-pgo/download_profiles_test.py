#!/usr/bin/env python3

# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import contextlib
import io
import os
import unittest

from tempfile import TemporaryDirectory
from unittest.mock import patch, mock_open

from download_profiles import main, parse_args, retrieve_version


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


class TestRetrieveVersion(unittest.TestCase):

  def mock_version_file(self, major, minor, build, patch):
    return (
        f'#define V8_MAJOR_VERSION {major}\n'
        f'#define V8_MINOR_VERSION {minor}\n'
        f'#define V8_BUILD_NUMBER {build}\n'
        f'#define V8_PATCH_LEVEL {patch}\n'
    )

  def test_retrieve_valid_version(self):
    with patch(
        'builtins.open',
        new=mock_open(read_data=self.mock_version_file(11, 4, 1, 0))):
      args = parse_args(['download'])
      version = retrieve_version(args)

    self.assertEqual(version, '11.4.1.0')

  def test_retrieve_parameter_version(self):
    args = parse_args(['download', '--version', '11.1.1.42'])
    version = retrieve_version(args)
    self.assertEqual(version, '11.1.1.42')

  def test_retrieve_untagged_version(self):
    with patch(
        'builtins.open',
        new=mock_open(read_data=self.mock_version_file(11, 4, 0, 0))), \
        self.assertRaises(SystemExit) as se:
      args = parse_args(['download'])
      version = retrieve_version(args)

    self.assertEqual(se.exception.code, 0)

if __name__ == '__main__':
  unittest.main()
