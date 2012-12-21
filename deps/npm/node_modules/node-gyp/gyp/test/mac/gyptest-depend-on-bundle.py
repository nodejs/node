#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a dependency on a bundle causes the whole bundle to be built.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='depend-on-bundle')

  test.build('test.gyp', 'dependent_on_bundle', chdir='depend-on-bundle')

  # Binary itself.
  test.built_file_must_exist('dependent_on_bundle', chdir='depend-on-bundle')

  # Bundle dependency.
  test.built_file_must_exist(
      'my_bundle.framework/Versions/A/my_bundle',
      chdir='depend-on-bundle')
  test.built_file_must_exist(  # package_framework
      'my_bundle.framework/my_bundle',
      chdir='depend-on-bundle')
  test.built_file_must_exist(  # plist
      'my_bundle.framework/Versions/A/Resources/Info.plist',
      chdir='depend-on-bundle')
  test.built_file_must_exist(
      'my_bundle.framework/Versions/A/Resources/English.lproj/'  # Resources
      'InfoPlist.strings',
      chdir='depend-on-bundle')

  test.pass_test()
