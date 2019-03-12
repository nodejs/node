#!/usr/bin/env python

# Copyright (c) 2016 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that Windows drivers are built correctly.
"""

import sys

if sys.platform != 'win32':
  print("Test only for Windows")
  sys.exit(2)

import TestGyp
from SConsLib import TestCmd

test = TestGyp.TestGyp(formats=['msvs'])

CHDIR = 'win-driver-target-type'
test.run_gyp('win-driver-target-type.gyp', chdir=CHDIR)
maybe_missing = r'[\s\S]+?(WindowsKernelModeDriver|Build succeeded.)[\s\S]+?'
test.build('win-driver-target-type.gyp', 'win_driver_target_type',
           chdir=CHDIR, stdout=maybe_missing,
           status=[0, 1], match=TestCmd.match_re_dotall)


test.pass_test()
