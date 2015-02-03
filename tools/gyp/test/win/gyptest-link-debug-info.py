#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure debug info setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('debug-info.gyp', chdir=CHDIR)
  test.build('debug-info.gyp', test.ALL, chdir=CHDIR)

  suffix = '.exe.pdb' if test.format == 'ninja' else '.pdb'
  test.built_file_must_not_exist('test_debug_off%s' % suffix, chdir=CHDIR)
  test.built_file_must_exist('test_debug_on%s' % suffix, chdir=CHDIR)

  test.pass_test()
