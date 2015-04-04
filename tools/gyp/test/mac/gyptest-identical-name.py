#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies libraries (in identical-names) are properly handeled by xcode.

The names for all libraries participating in this build are:
libtestlib.a - identical-name/testlib
libtestlib.a - identical-name/proxy/testlib
libproxy.a   - identical-name/proxy
The first two libs produce a hash collision in Xcode when Gyp is executed,
because they have the same name and would be copied to the same directory with
Xcode default settings.
For this scenario to work one needs to change the Xcode variables SYMROOT and
CONFIGURATION_BUILD_DIR. Setting these to per-lib-unique directories, avoids
copying the libs into the same directory.

The test consists of two steps. The first one verifies that by setting both
vars, there is no hash collision anymore during Gyp execution and that the libs
can actually be be built. The second one verifies that there is still a hash
collision if the vars are not set and thus the current behavior is preserved.
"""

import TestGyp

import sys

def IgnoreOutput(string, expected_string):
  return True

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['xcode'])


  test.run_gyp('test.gyp', chdir='identical-name')
  test.build('test.gyp', test.ALL, chdir='identical-name')

  test.run_gyp('test-should-fail.gyp', chdir='identical-name')
  test.built_file_must_not_exist('test-should-fail.xcodeproj')

  test.pass_test()

