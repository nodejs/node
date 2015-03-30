#!/usr/bin/env python

# Copyright (c) 2013 Yandex LLC. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure manifests are embedded in binaries properly. Handling of
AdditionalManifestFiles is tested too.
"""

import TestGyp

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
        return win32api.LoadResource(handle, RT_MANIFEST, resource_name)
      except pywintypes.error as error:
        if error.args[0] == winerror.ERROR_RESOURCE_DATA_NOT_FOUND:
          return None
        else:
          raise

  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])
  CHDIR = 'linker-flags'
  test.run_gyp('embed-manifest.gyp', chdir=CHDIR)
  test.build('embed-manifest.gyp', test.ALL, chdir=CHDIR)

  # The following binaries must contain a manifest embedded.
  test.fail_test(not extract_manifest(test.built_file_path(
    'test_manifest_exe.exe', chdir=CHDIR), 1))
  test.fail_test(not extract_manifest(test.built_file_path(
    'test_manifest_exe_inc.exe', chdir=CHDIR), 1))
  test.fail_test(not extract_manifest(test.built_file_path(
    'test_manifest_dll.dll', chdir=CHDIR), 2))
  test.fail_test(not extract_manifest(test.built_file_path(
    'test_manifest_dll_inc.dll', chdir=CHDIR), 2))

  # Must contain the Win7 support GUID, but not the Vista one (from
  # extra2.manifest).
  test.fail_test(
    '35138b9a-5d96-4fbd-8e2d-a2440225f93a' not in
    extract_manifest(test.built_file_path('test_manifest_extra1.exe',
                                            chdir=CHDIR), 1))
  test.fail_test(
    'e2011457-1546-43c5-a5fe-008deee3d3f0' in
    extract_manifest(test.built_file_path('test_manifest_extra1.exe',
                                            chdir=CHDIR), 1))
  # Must contain both.
  test.fail_test(
    '35138b9a-5d96-4fbd-8e2d-a2440225f93a' not in
    extract_manifest(test.built_file_path('test_manifest_extra2.exe',
                                            chdir=CHDIR), 1))
  test.fail_test(
    'e2011457-1546-43c5-a5fe-008deee3d3f0' not in
    extract_manifest(test.built_file_path('test_manifest_extra2.exe',
                                            chdir=CHDIR), 1))

  # Same as extra2, but using list syntax instead.
  test.fail_test(
    '35138b9a-5d96-4fbd-8e2d-a2440225f93a' not in
    extract_manifest(test.built_file_path('test_manifest_extra_list.exe',
                                          chdir=CHDIR), 1))
  test.fail_test(
    'e2011457-1546-43c5-a5fe-008deee3d3f0' not in
    extract_manifest(test.built_file_path('test_manifest_extra_list.exe',
                                          chdir=CHDIR), 1))

  # Test that incremental linking doesn't force manifest embedding.
  test.fail_test(extract_manifest(test.built_file_path(
    'test_manifest_exe_inc_no_embed.exe', chdir=CHDIR), 1))

  test.pass_test()
