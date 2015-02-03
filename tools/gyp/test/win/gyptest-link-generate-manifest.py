#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure we generate a manifest file when linking binaries, including
handling AdditionalManifestFiles.
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
  test.run_gyp('generate-manifest.gyp', chdir=CHDIR)
  test.build('generate-manifest.gyp', test.ALL, chdir=CHDIR)

  # Make sure that generation of .generated.manifest does not cause a relink.
  test.run_gyp('generate-manifest.gyp', chdir=CHDIR)
  test.up_to_date('generate-manifest.gyp', test.ALL, chdir=CHDIR)

  def test_manifest(filename, generate_manifest, embedded_manifest,
                    extra_manifest):
    exe_file = test.built_file_path(filename, chdir=CHDIR)
    if not generate_manifest:
      test.must_not_exist(exe_file + '.manifest')
      manifest = extract_manifest(exe_file, 1)
      test.fail_test(manifest)
      return
    if embedded_manifest:
      manifest = extract_manifest(exe_file, 1)
      test.fail_test(not manifest)
    else:
      test.must_exist(exe_file + '.manifest')
      manifest = test.read(exe_file + '.manifest')
      test.fail_test(not manifest)
      test.fail_test(extract_manifest(exe_file, 1))
    if generate_manifest:
      test.must_contain_any_line(manifest, 'requestedExecutionLevel')
    if extra_manifest:
      test.must_contain_any_line(manifest,
                                 '35138b9a-5d96-4fbd-8e2d-a2440225f93a')
      test.must_contain_any_line(manifest,
                                 'e2011457-1546-43c5-a5fe-008deee3d3f0')

  test_manifest('test_generate_manifest_true.exe',
                generate_manifest=True,
                embedded_manifest=False,
                extra_manifest=False)
  test_manifest('test_generate_manifest_false.exe',
                generate_manifest=False,
                embedded_manifest=False,
                extra_manifest=False)
  test_manifest('test_generate_manifest_default.exe',
                generate_manifest=True,
                embedded_manifest=False,
                extra_manifest=False)
  test_manifest('test_generate_manifest_true_as_embedded.exe',
                generate_manifest=True,
                embedded_manifest=True,
                extra_manifest=False)
  test_manifest('test_generate_manifest_false_as_embedded.exe',
                generate_manifest=False,
                embedded_manifest=True,
                extra_manifest=False)
  test_manifest('test_generate_manifest_default_as_embedded.exe',
                generate_manifest=True,
                embedded_manifest=True,
                extra_manifest=False)
  test_manifest('test_generate_manifest_true_with_extra_manifest.exe',
                generate_manifest=True,
                embedded_manifest=False,
                extra_manifest=True)
  test_manifest('test_generate_manifest_false_with_extra_manifest.exe',
                generate_manifest=False,
                embedded_manifest=False,
                extra_manifest=True)
  test_manifest('test_generate_manifest_true_with_extra_manifest_list.exe',
                generate_manifest=True,
                embedded_manifest=False,
                extra_manifest=True)
  test_manifest('test_generate_manifest_false_with_extra_manifest_list.exe',
                generate_manifest=False,
                embedded_manifest=False,
                extra_manifest=True)
  test_manifest('test_generate_manifest_default_embed_default.exe',
                generate_manifest=True,
                embedded_manifest=True,
                extra_manifest=False)
  test.pass_test()
