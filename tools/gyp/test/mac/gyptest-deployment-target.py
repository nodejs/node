#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that MACOSX_DEPLOYMENT_TARGET works.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['make', 'ninja', 'xcode'])

  if test.format == 'make':
    # This is failing because of a deprecation warning for libstdc++.
    test.skip_test()  # bug=533

  test.run_gyp('deployment-target.gyp', chdir='deployment-target')

  test.build('deployment-target.gyp', test.ALL, chdir='deployment-target')

  test.pass_test()

