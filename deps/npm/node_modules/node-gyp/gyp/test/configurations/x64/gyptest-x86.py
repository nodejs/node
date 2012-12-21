#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of an executable in three different configurations.
"""

import TestGyp

import sys

formats = ['msvs']
if sys.platform == 'win32':
  formats += ['ninja']
test = TestGyp.TestGyp(formats=formats)

test.run_gyp('configurations.gyp')
test.set_configuration('Debug|Win32')
test.build('configurations.gyp', test.ALL)

for machine, suffix in [('14C machine (x86)', ''),
                        ('8664 machine (x64)', '64')]:
  output = test.run_dumpbin(
      '/headers', test.built_file_path('configurations%s.exe' % suffix))
  if machine not in output:
    test.fail_test()

test.pass_test()
