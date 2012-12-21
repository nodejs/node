#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that the 'Profile' attribute in VCLinker is extracted properly.
"""

import TestGyp

import os
import sys


if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])
  CHDIR = 'linker-flags'
  test.run_gyp('program-database.gyp', chdir=CHDIR)
  test.build('program-database.gyp', test.ALL, chdir=CHDIR)

  def FindFile(pdb):
    full_path = test.built_file_path(pdb, chdir=CHDIR)
    return os.path.isfile(full_path)

  # Verify the specified PDB is created when ProgramDatabaseFile
  # is provided.
  if not FindFile('name_outdir.pdb'):
    test.fail_test()
  if not FindFile('name_proddir.pdb'):
    test.fail_test()

  test.pass_test()
