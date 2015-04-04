#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Checks that gyp succeeds on static_library targets which have several files with
the same basename.
"""

import os
import sys

import TestGyp

test = TestGyp.TestGyp()

if ((test.format == 'make' and sys.platform == 'darwin') or
    (test.format == 'msvs' and
        int(os.environ.get('GYP_MSVS_VERSION', 2010)) < 2010)):
  test.run_gyp('double-static.gyp',
               chdir='src', status=1, stderr=None)
else:
  test.run_gyp('double-static.gyp',
               chdir='src')
  test.build('double-static.gyp', test.ALL, chdir='src')

test.pass_test()
