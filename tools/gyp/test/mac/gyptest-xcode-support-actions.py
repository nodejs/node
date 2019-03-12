#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that support actions are properly created.
"""

import TestGyp

import os
import subprocess
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['xcode'])

  CHDIR = 'xcode-support-actions'

  test.run_gyp('test.gyp', '-Gsupport_target_suffix=_customsuffix', chdir=CHDIR)
  test.build('test.gyp', target='target_customsuffix', chdir=CHDIR)

  test.pass_test()
