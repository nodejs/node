#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that bundles that have no 'sources' (pure resource containers) work.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='sourceless-module')

  # Just needs to build without errors.
  test.build('test.gyp', 'empty_bundle', chdir='sourceless-module')
  test.built_file_must_not_exist(
      'empty_bundle.bundle', chdir='sourceless-module')

  # Needs to build, and contain a resource.
  test.build('test.gyp', 'resource_bundle', chdir='sourceless-module')

  test.built_file_must_exist(
      'resource_bundle.bundle/Contents/Resources/foo.manifest',
      chdir='sourceless-module')
  test.built_file_must_not_exist(
      'resource_bundle.bundle/Contents/MacOS/resource_bundle',
      chdir='sourceless-module')

  # Needs to build and cause the bundle to be built.
  test.build(
      'test.gyp', 'dependent_on_resource_bundle', chdir='sourceless-module')

  test.built_file_must_exist(
      'resource_bundle.bundle/Contents/Resources/foo.manifest',
      chdir='sourceless-module')
  test.built_file_must_not_exist(
      'resource_bundle.bundle/Contents/MacOS/resource_bundle',
      chdir='sourceless-module')

  test.pass_test()
