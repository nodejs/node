#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure pdb is named as expected (shared between .cc files).
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('pdbname.gyp', chdir=CHDIR)
  test.build('pdbname.gyp', test.ALL, chdir=CHDIR)

  # Confirm that the default behaviour is to name the .pdb per-target (rather
  # than per .cc file).
  test.built_file_must_exist('test_pdbname.pdb', chdir=CHDIR)

  test.pass_test()
