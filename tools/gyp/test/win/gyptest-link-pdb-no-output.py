#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Ensure that when debug information is not output, a pdb is not expected.
"""

import TestGyp

import os
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp()
  CHDIR = 'linker-flags'
  test.run_gyp('pdb-output.gyp', chdir=CHDIR)
  test.build('pdb-output.gyp', 'test_pdb_output_disabled', chdir=CHDIR)
  # Make sure that the build doesn't expect a PDB to be generated when there
  # will be none.
  test.up_to_date('pdb-output.gyp', 'test_pdb_output_disabled', chdir=CHDIR)

  test.pass_test()
