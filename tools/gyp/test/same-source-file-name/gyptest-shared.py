#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Checks that gyp succeeds on shared_library targets which have several files with
the same basename.
"""

import os

import TestGyp

test = TestGyp.TestGyp()

if ((test.format == 'msvs') and
       (int(os.environ.get('GYP_MSVS_VERSION', 2010)) < 2010)):
  test.run_gyp('double-shared.gyp',
               chdir='src', status=0, stderr=None)
else:
  test.run_gyp('double-shared.gyp',
               chdir='src')
  test.build('double-shared.gyp', test.ALL, chdir='src')

test.pass_test()
