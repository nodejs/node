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

  # Build an app containing an actionless bundle.
  test.build(
      'test.gyp',
      'bundle_dependent_on_resource_bundle_no_actions',
      chdir='sourceless-module')

  test.built_file_must_exist(
      'bundle_dependent_on_resource_bundle_no_actions.app/Contents/Resources/'
          'mac_resource_bundle_no_actions.bundle/Contents/Resources/empty.txt',
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

  # TODO(thakis): shared_libraries that have no sources but depend on static
  # libraries currently only work with the ninja generator.  This is used by
  # chrome/mac's components build.
  if test.format == 'ninja':
    # Check that an executable depending on a resource framework links fine too.
    test.build(
       'test.gyp', 'dependent_on_resource_framework', chdir='sourceless-module')

    test.built_file_must_exist(
        'resource_framework.framework/Resources/foo.manifest',
        chdir='sourceless-module')
    test.built_file_must_exist(
        'resource_framework.framework/resource_framework',
        chdir='sourceless-module')

  test.pass_test()
