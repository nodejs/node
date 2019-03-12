#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure binary is relinked when manifest settings are changed.
"""

import TestGyp

import os
import sys

if sys.platform == 'win32':
  import pywintypes
  import win32api
  import winerror

  RT_MANIFEST = 24

  class LoadLibrary(object):
    """Context manager for loading and releasing binaries in Windows.
    Yields the handle of the binary loaded."""
    def __init__(self, path):
      self._path = path
      self._handle = None

    def __enter__(self):
      self._handle = win32api.LoadLibrary(self._path)
      return self._handle

    def __exit__(self, type, value, traceback):
      win32api.FreeLibrary(self._handle)

  def extract_manifest(path, resource_name):
    """Reads manifest from |path| and returns it as a string.
    Returns None is there is no such manifest."""
    with LoadLibrary(path) as handle:
      try:
        return win32api.LoadResource(
            handle, RT_MANIFEST, resource_name).decode('utf-8', 'ignore')
      except pywintypes.error as error:
        if error.args[0] == winerror.ERROR_RESOURCE_DATA_NOT_FOUND:
          return None
        else:
          raise

  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'

  gyp_template = '''
{
 'targets': [
    {
      'target_name': 'test_update_manifest',
      'type': 'executable',
      'sources': ['hello.cc'],
      'msvs_settings': {
        'VCLinkerTool': {
          'EnableUAC': 'true',
          'UACExecutionLevel': '%(uac_execution_level)d',
        },
        'VCManifestTool': {
          'EmbedManifest': 'true',
          'AdditionalManifestFiles': '%(additional_manifest_files)s',
        },
      },
    },
  ],
}
'''

  gypfile = 'update-manifest.gyp'

  def WriteAndUpdate(uac_execution_level, additional_manifest_files, do_build):
    with open(os.path.join(CHDIR, gypfile), 'w') as f:
      f.write(gyp_template % {
        'uac_execution_level': uac_execution_level,
        'additional_manifest_files': additional_manifest_files,
      })
    test.run_gyp(gypfile, chdir=CHDIR)
    if do_build:
      test.build(gypfile, chdir=CHDIR)
      exe_file = test.built_file_path('test_update_manifest.exe', chdir=CHDIR)
      return extract_manifest(exe_file, 1)

  manifest = WriteAndUpdate(0, '', True)
  test.fail_test('asInvoker' not in manifest)
  test.fail_test('35138b9a-5d96-4fbd-8e2d-a2440225f93a' in manifest)

  # Make sure that updating .gyp and regenerating doesn't cause a rebuild.
  WriteAndUpdate(0, '', False)
  test.up_to_date(gypfile, test.ALL, chdir=CHDIR)

  # But make sure that changing a manifest property does cause a relink.
  manifest = WriteAndUpdate(2, '', True)
  test.fail_test('requireAdministrator' not in manifest)

  # Adding a manifest causes a rebuild.
  manifest = WriteAndUpdate(2, 'extra.manifest', True)
  test.fail_test('35138b9a-5d96-4fbd-8e2d-a2440225f93a' not in manifest)
