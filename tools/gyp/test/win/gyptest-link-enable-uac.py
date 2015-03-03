#!/usr/bin/env python

# Copyright 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that embedding UAC information into the manifest works.
"""

import TestGyp

import sys
from xml.dom.minidom import parseString

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
  test.run_gyp('enable-uac.gyp', chdir=CHDIR)
  test.build('enable-uac.gyp', test.ALL, chdir=CHDIR)

  # The following binaries must contain a manifest embedded.
  test.fail_test(not extract_manifest(test.built_file_path(
    'enable_uac.exe', chdir=CHDIR), 1))
  test.fail_test(not extract_manifest(test.built_file_path(
    'enable_uac_no.exe', chdir=CHDIR), 1))
  test.fail_test(not extract_manifest(test.built_file_path(
    'enable_uac_admin.exe', chdir=CHDIR), 1))

  # Verify that <requestedExecutionLevel level="asInvoker" uiAccess="false" />
  # is present.
  manifest = parseString(extract_manifest(
      test.built_file_path('enable_uac.exe', chdir=CHDIR), 1))
  execution_level = manifest.getElementsByTagName('requestedExecutionLevel')
  test.fail_test(len(execution_level) != 1)
  execution_level = execution_level[0].attributes
  test.fail_test(not (
      execution_level.has_key('level') and
      execution_level.has_key('uiAccess') and
      execution_level['level'].nodeValue == 'asInvoker' and
      execution_level['uiAccess'].nodeValue == 'false'))

  # Verify that <requestedExecutionLevel> is not in the menifest.
  manifest = parseString(extract_manifest(
      test.built_file_path('enable_uac_no.exe', chdir=CHDIR), 1))
  execution_level = manifest.getElementsByTagName('requestedExecutionLevel')
  test.fail_test(len(execution_level) != 0)

  # Verify that <requestedExecutionLevel level="requireAdministrator"
  # uiAccess="true" /> is present.
  manifest = parseString(extract_manifest(
      test.built_file_path('enable_uac_admin.exe', chdir=CHDIR), 1))
  execution_level = manifest.getElementsByTagName('requestedExecutionLevel')
  test.fail_test(len(execution_level) != 1)
  execution_level = execution_level[0].attributes
  test.fail_test(not (
      execution_level.has_key('level') and
      execution_level.has_key('uiAccess') and
      execution_level['level'].nodeValue == 'requireAdministrator' and
      execution_level['uiAccess'].nodeValue == 'true'))

  test.pass_test()
