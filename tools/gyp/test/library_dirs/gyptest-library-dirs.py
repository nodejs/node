#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies library_dirs (in link_settings) are properly found.
"""

import sys

import TestGyp

test = TestGyp.TestGyp(formats=['!android'])

lib_dir = test.tempdir('secret_location')

test.run_gyp('test.gyp',
             '-D', 'abs_path_to_secret_library_location={0}'.format(lib_dir),
             chdir='subdir')

# Must build each target independently, since they are not in each others'
# 'dependencies' (test.ALL does NOT work here for some builders, and in any case
# would not ensure the correct ordering).
test.build('test.gyp', 'mylib', chdir='subdir')
test.build('test.gyp', 'libraries-search-path-test', chdir='subdir')

expect = """Hello world
"""
test.run_built_executable(
    'libraries-search-path-test', chdir='subdir', stdout=expect)

if sys.platform in ('win32', 'cygwin'):
  test.run_gyp('test-win.gyp',
               '-D',
               'abs_path_to_secret_library_location={0}'.format(lib_dir),
               chdir='subdir')

  test.build('test.gyp', 'mylib', chdir='subdir')
  test.build('test-win.gyp',
             'libraries-search-path-test-lib-suffix',
             chdir='subdir')

  test.run_built_executable(
        'libraries-search-path-test-lib-suffix', chdir='subdir', stdout=expect)


test.pass_test()
test.cleanup()
