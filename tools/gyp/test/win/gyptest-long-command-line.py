#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure long command lines work.
"""

import TestGyp

import subprocess
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja', 'msvs'])

  CHDIR = 'long-command-line'
  test.run_gyp('long-command-line.gyp', chdir=CHDIR)
  test.build('long-command-line.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
