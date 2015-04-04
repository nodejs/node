#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure batch files run as actions. Regression test for previously missing
trailing quote on command line. cmd typically will implicitly insert a missing
quote, but if the command ends in a quote, it will not insert another, so the
command can sometimes become unterminated.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'batch-file-action'
  test.run_gyp('batch-file-action.gyp', chdir=CHDIR)
  test.build('batch-file-action.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
