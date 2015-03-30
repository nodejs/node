#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ios app bundles are built correctly.
"""

import TestGyp
import TestMac

import os.path
import sys

# Xcode supports for assets catalog was introduced in Xcode 6.0
if sys.platform == 'darwin' and TestMac.Xcode.Version() >= '0600':
  test_gyp_path = 'test-assets-catalog.gyp'
  test_app_path = 'Test App Assets Catalog Gyp.app'

  test = TestGyp.TestGyp(formats=['xcode', 'ninja'])
  test.run_gyp(test_gyp_path, chdir='app-bundle')
  test.build(test_gyp_path, test.ALL, chdir='app-bundle')

  # Test that the extension is .bundle
  test.built_file_must_exist(
      os.path.join(test_app_path, 'Test App Assets Catalog Gyp'),
      chdir='app-bundle')

  # Info.plist
  info_plist = test.built_file_path(
      os.path.join(test_app_path, 'Info.plist'),
      chdir='app-bundle')
  # Resources
  test.built_file_must_exist(
      os.path.join(test_app_path, 'English.lproj/InfoPlist.strings'),
      chdir='app-bundle')
  test.built_file_must_exist(
      os.path.join(test_app_path, 'English.lproj/MainMenu.nib'),
      chdir='app-bundle')
  test.built_file_must_exist(
      os.path.join(test_app_path, 'English.lproj/Main_iPhone.storyboardc'),
      chdir='app-bundle')
  test.built_file_must_exist(
      os.path.join(test_app_path, 'Assets.car'),
      chdir='app-bundle')

  # Packaging
  test.built_file_must_exist(
      os.path.join(test_app_path, 'PkgInfo'),
      chdir='app-bundle')
  test.built_file_must_match(
      os.path.join(test_app_path, 'PkgInfo'), 'APPLause',
      chdir='app-bundle')

  test.pass_test()
