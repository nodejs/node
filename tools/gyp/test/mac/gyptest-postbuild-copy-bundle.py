#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a postbuild copying a dependend framework into an app bundle is
rerun if the resources in the framework change.
"""

import TestGyp

import os.path
import sys

if sys.platform == 'darwin':
  # TODO(thakis): Make this pass with the make generator, http://crbug.com/95529
  test = TestGyp.TestGyp(formats=['ninja', 'xcode'])

  CHDIR = 'postbuild-copy-bundle'
  test.run_gyp('test.gyp', chdir=CHDIR)

  app_bundle_dir = test.built_file_path('Test App.app', chdir=CHDIR)
  bundled_framework_dir = os.path.join(
      app_bundle_dir, 'Contents', 'My Framework.framework', 'Resources')
  final_plist_path = os.path.join(bundled_framework_dir, 'Info.plist')
  final_resource_path = os.path.join(bundled_framework_dir, 'resource_file.sb')
  final_copies_path = os.path.join(
      app_bundle_dir, 'Contents', 'My Framework.framework', 'Versions', 'A',
      'Libraries', 'copied.txt')

  # Check that the dependency was built and copied into the app bundle:
  test.build('test.gyp', 'test_app', chdir=CHDIR)
  test.must_exist(final_resource_path)
  test.must_match(final_resource_path,
                  'This is included in the framework bundle.\n')

  test.must_exist(final_plist_path)
  test.must_contain(final_plist_path, '''\
\t<key>RandomKey</key>
\t<string>RandomValue</string>''')

  # Touch the dependency's bundle resource, and check that the modification
  # makes it all the way into the app bundle:
  test.sleep()
  test.write('postbuild-copy-bundle/resource_file.sb', 'New text\n')
  test.build('test.gyp', 'test_app', chdir=CHDIR)

  test.must_exist(final_resource_path)
  test.must_match(final_resource_path, 'New text\n')

  # Check the same for the plist file.
  test.sleep()
  contents = test.read('postbuild-copy-bundle/Framework-Info.plist')
  contents = contents.replace('RandomValue', 'NewRandomValue')
  test.write('postbuild-copy-bundle/Framework-Info.plist', contents)
  test.build('test.gyp', 'test_app', chdir=CHDIR)

  test.must_exist(final_plist_path)
  test.must_contain(final_plist_path, '''\
\t<key>RandomKey</key>
\t<string>NewRandomValue</string>''')

  # Check the same for the copies section, test for http://crbug.com/157077
  test.sleep()
  contents = test.read('postbuild-copy-bundle/copied.txt')
  contents = contents.replace('old', 'new')
  test.write('postbuild-copy-bundle/copied.txt', contents)
  test.build('test.gyp', 'test_app', chdir=CHDIR)

  test.must_exist(final_copies_path)
  test.must_contain(final_copies_path, 'new copied file')

  test.pass_test()
