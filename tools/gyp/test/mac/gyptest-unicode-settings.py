#!/usr/bin/env python

# Copyright 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that unicode strings in 'xcode_settings' work.
Also checks that ASCII control characters are escaped properly.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['xcode'])
  test.run_gyp('test.gyp', chdir='unicode-settings')
  test.build('test.gyp', test.ALL, chdir='unicode-settings')
  test.pass_test()
