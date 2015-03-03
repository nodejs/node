#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable in three different configurations.
"""

import TestGyp

test = TestGyp.TestGyp()

if test.format == 'android':
  # This test currently fails on android. Investigate why, fix the issues
  # responsible, and reenable this test on android. See bug:
  # https://code.google.com/p/gyp/issues/detail?id=436
  test.skip_test(message='Test fails on android. Fix and reenable.\n')

test.run_gyp('configurations.gyp')

test.set_configuration('Release')
test.build('configurations.gyp')
test.run_built_executable('configurations',
                          stdout=('Base configuration\n'
                                  'Common configuration\n'
                                  'Common2 configuration\n'
                                  'Release configuration\n'))

test.set_configuration('Debug')
test.build('configurations.gyp')
test.run_built_executable('configurations',
                          stdout=('Base configuration\n'
                                  'Common configuration\n'
                                  'Common2 configuration\n'
                                  'Debug configuration\n'))

test.pass_test()
