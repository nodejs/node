#!/usr/bin/env python3

# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import contextlib
import io
import json
import os
import pathlib
import re
import unittest

from tempfile import TemporaryDirectory
from unittest.mock import patch, mock_open

from download_profiles import ProfileDownloader


class BaseTestCase(unittest.TestCase):
  def setUp(self):
    self.chromium_dir = TemporaryDirectory()
    self.chromium_path = pathlib.Path(self.chromium_dir.name)

    os.makedirs(self.profiles_path)

    patch('download_profiles.PGO_PROFILE_DIR', self.profiles_path).start()
    patch('download_profiles.V8_DIR', self.chromium_path / 'v8').start()
    patch('download_profiles.VERSION_FILE', self.version_h_path).start()

  def tearDown(self):
    patch.stopall()
    self.chromium_dir.cleanup()

  def add_version_h_file(self, major, minor, build=0, patch=0):
    self.version_h_path.parents[0].mkdir(parents=True, exist_ok=True)

    with self.version_h_path.open('w') as f:
      f.write(
        f'#define V8_MAJOR_VERSION {major}\n'
        f'#define V8_MINOR_VERSION {minor}\n'
        f'#define V8_BUILD_NUMBER {build}\n'
        f'#define V8_PATCH_LEVEL {patch}\n'
      )

  @property
  def v8_path(self):
    return self.chromium_path / 'v8'

  @property
  def profiles_path(self):
    return self.v8_path / 'tools/builtins-pgo/profiles'

  @property
  def version_h_path(self):
    return self.v8_path / 'include/v8-version.h'


class TestDownloadProfiles(BaseTestCase):
  _FAKE_REVISION = '778cc4ae9ebb1973e900d4a56a16e6415492ab1d'

  def _fake_call_gsutil(self, downloader, cmd, failure_hint):
    command = ' '.join(cmd)
    if 'by-version/0.0.0.42/' in command:
      downloader._print_error(['gsutil', *cmd], 404, '', '', failure_hint)
      raise SystemExit(4)

    if cmd[0] == 'stat':
      return

    target_dir = pathlib.Path(cmd[-1])
    target_dir.mkdir(parents=True, exist_ok=True)
    match = re.search(r'by-version/([^/]+)/\*', command)
    version = match.group(1) if match else downloader.version
    with (target_dir / 'meta.json').open('w') as f:
      json.dump({'version': version, 'revision': self._FAKE_REVISION}, f)
    (target_dir / 'x64.profile').write_text('fake')
    (target_dir / 'x86.profile').write_text('fake')

  def _mock_gsutil(self):

    def patched_call_gsutil(downloader, cmd, failure_hint):
      return self._fake_call_gsutil(downloader, cmd, failure_hint)

    return patch.object(
        ProfileDownloader, '_call_gsutil', new=patched_call_gsutil)

  def _test_cmd(self, cmd, exitcode):
    out = io.StringIO()
    err = io.StringIO()
    with self.assertRaises(SystemExit) as se, \
        contextlib.redirect_stdout(out), \
        contextlib.redirect_stderr(err):
      downloader = ProfileDownloader(cmd)

      # Note: This loads the version file before running the downloader to
      # simplify patching.
      downloader.version

      downloader.run()

    self.assertEqual(se.exception.code, exitcode)
    return out.getvalue(), err.getvalue()

  def test_validate_profiles(self):
    with self._mock_gsutil():
      out, err = self._test_cmd(['validate', '--version', '11.1.0.0'], 0)
    self.assertEqual(len(out), 0)
    self.assertEqual(len(err), 0)

  def test_download_profiles(self):
    with self._mock_gsutil():
      out, err = self._test_cmd(['download', '--version', '11.1.0.0'], 0)
    self.assertEqual(len(out), 0)
    self.assertEqual(len(err), 0)
    self.assertTrue(any(
        f.name.endswith('.profile') for f in self.profiles_path.glob('*')))

    with (self.profiles_path / 'meta.json').open() as f:
      self.assertEqual(json.load(f)['version'], '11.1.0.0')

    # A second download should not be started as profiles exist already
    with patch('download_profiles.ProfileDownloader._call_gsutil') as gsutil:
      out, err = self._test_cmd(['download', '--version', '11.1.0.0'], 0)
      self.assertEqual(out,
          'Profiles already downloaded, use --force to overwrite.\n')
      gsutil.assert_not_called()

    # A forced download should always trigger
    with patch('download_profiles.ProfileDownloader._call_gsutil') as gsutil:
      cmd = ['download', '--version', '11.1.0.0', '--force']
      out, err = self._test_cmd(cmd, 0)
      self.assertEqual(len(out), 0)
      self.assertEqual(len(err), 0)
      gsutil.assert_called_once_with([
          '-m', 'cp', '-R',
          'gs://chromium-v8-builtins-pgo/by-version/11.1.0.0/*',
          str(self.profiles_path)
      ], 'https://storage.googleapis.com/chromium-v8-builtins-pgo/by-version/'
                                     '11.1.0.0 does not exist.')

  def test_arg_quiet(self):
    self.add_version_h_file(11, 9)

    out, err = self._test_cmd(['download'], 0)
    self.assertGreater(len(out), 0)

    out, err = self._test_cmd(['download', '--quiet'], 0)
    self.assertEqual(len(out), 0)

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
    with self._mock_gsutil():
      out, err = self._test_cmd(['download', '--version', '0.0.0.42'], 4)
    self.assertEqual(len(out), 0)
    self.assertGreater(len(err), 0)

  def test_chromium_deps_valid_v8_revision(self):
    with (self.chromium_path / 'DEPS').open('w') as f:
      f.write(f"'v8_revision': '{self._FAKE_REVISION}',")

    cmd = ['download', '--version', '11.1.0.0', '--check-v8-revision']
    with self._mock_gsutil():
      self._test_cmd(cmd, 0)

  def test_chromium_deps_no_file(self):
    cmd = ['download', '--version', '11.1.0.0', '--check-v8-revision']
    with self._mock_gsutil():
      self._test_cmd(cmd, 7)

  def test_chromium_deps_no_v8_revision(self):
    with (self.chromium_path / 'DEPS').open('w') as f:
      pass

    cmd = ['download', '--version', '11.1.0.0', '--check-v8-revision']
    with self._mock_gsutil():
      self._test_cmd(cmd, 6)

  def test_chromium_deps_invalid_v8_revision(self):
    with (self.chromium_path / 'DEPS').open('w') as f:
      f.write("'v8_revision': 'deadbeefdeadbeefdeadbeefdeadbeefdeadbeef',")

    cmd = ['download', '--version', '11.1.0.0', '--check-v8-revision']
    with self._mock_gsutil():
      self._test_cmd(cmd, 5)

  def test_call_gsutil_respects_quiet_mode(self):
    cmd = ['stat', 'gs://test/meta.json']
    failure_hint = 'hint'
    cases = [
        {
            'name': 'success_quiet_true',
            'quiet': True
        },
        {
            'name': 'success_quiet_false',
            'quiet': False
        },
        {
            'name': 'quiet_failure_includes_stdout_stderr',
            'quiet': True,
            'check_call_return': (1, 'quiet-out', 'quiet-err'),
            'expect_exit': 4,
            'expect_in_stderr': ['stdout:', 'stderr:'],
        },
        {
            'name': 'default_failure_omits_stdout_stderr',
            'quiet': False,
            'call_return': 1,
            'expect_exit': 4,
            'expect_not_in_stderr': ['stdout:', 'stderr:'],
        },
    ]
    for case in cases:
      with (
          self.subTest(case['name']),
          patch.object(ProfileDownloader, '_import_gsutil'),
          patch('download_profiles.gcs_download', create=True) as gcs_download,
      ):
        gsutil = gcs_download.Gsutil.return_value
        gsutil.check_call.return_value = case.get('check_call_return',
                                                  (0, 'out', 'err'))
        gsutil.call.return_value = case.get('call_return', 0)
        args = ['download', '--version', '11.1.0.0']
        if case['quiet']:
          args.append('--quiet')
        downloader = ProfileDownloader(args)
        expect_exit = case.get('expect_exit')
        if expect_exit is None:
          downloader._call_gsutil(cmd, failure_hint)
        else:
          err = io.StringIO()
          with contextlib.redirect_stderr(err), self.assertRaises(
              SystemExit) as se:
            downloader._call_gsutil(cmd, failure_hint)
          self.assertEqual(se.exception.code, expect_exit)
          stderr = err.getvalue()
          for pattern in case.get('expect_in_stderr', []):
            self.assertIn(pattern, stderr)
          for pattern in case.get('expect_not_in_stderr', []):
            self.assertNotIn(pattern, stderr)

        if case['quiet']:
          gsutil.check_call.assert_called_once_with(*cmd)
          gsutil.call.assert_not_called()
        else:
          gsutil.call.assert_called_once_with(*cmd)
          gsutil.check_call.assert_not_called()


class TestRetrieveVersion(BaseTestCase):
  def test_retrieve_valid_version(self):
    self.add_version_h_file(11, 4, 1)

    downloader = ProfileDownloader(['download'])
    self.assertEqual(downloader.version, '11.4.1.0')

  def test_retrieve_parameter_version(self):
    downloader = ProfileDownloader(['download', '--version', '11.1.1.42'])
    self.assertEqual(downloader.version, '11.1.1.42')

  def test_retrieve_untagged_version(self):
    self.add_version_h_file(11, 4)

    out = io.StringIO()
    with contextlib.redirect_stdout(out), self.assertRaises(SystemExit) as se:
      downloader = ProfileDownloader(['download'])
      downloader.version

    self.assertEqual(se.exception.code, 0)
    self.assertEqual(out.getvalue(),
        'The version file specifies 11.4.0.0, which has no profiles.\n')


if __name__ == '__main__':
  unittest.main()
