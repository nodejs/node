#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Ensure that ninja includes the .pdb as an output file from linking.
"""

import TestGyp

import os
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])
  CHDIR = 'linker-flags'
  test.run_gyp('pdb-output.gyp', chdir=CHDIR)
  # Note, building the pdbs rather than ALL or gyp target.
  test.build('pdb-output.gyp', 'output_exe.pdb', chdir=CHDIR)
  test.build('pdb-output.gyp', 'output_dll.pdb', chdir=CHDIR)

  def FindFile(pdb):
    full_path = test.built_file_path(pdb, chdir=CHDIR)
    return os.path.isfile(full_path)

  if not FindFile('output_exe.pdb'):
    test.fail_test()
  if not FindFile('output_dll.pdb'):
    test.fail_test()

  test.pass_test()
