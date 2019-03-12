#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test that MACH_O_TYPE etc are set correctly.
"""

import os
import sys

if sys.platform != 'darwin' or os.environ.get('TRAVIS') == 'true':
  print('only for macOS')
  sys.exit(2)

import TestGyp

test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

test.run_gyp('test.gyp', '-G', 'xcode_ninja_target_pattern=^(?!nonbundle_none).*$', chdir='type_envvars')

# The actual test is done by postbuild scripts during |test.build()|.
test.build('test.gyp', test.ALL, chdir='type_envvars')
