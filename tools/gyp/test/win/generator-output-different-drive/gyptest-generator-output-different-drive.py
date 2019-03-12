#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test that the generator output can be written to a different drive on Windows.
"""

import os
import TestGyp
import string
import subprocess
import sys


if sys.platform == 'win32':
  import win32api

  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  def GetFirstFreeDriveLetter():
    """ Returns the first unused Windows drive letter in [A, Z] """
    all_letters = [c for c in string.ascii_uppercase]
    in_use = win32api.GetLogicalDriveStrings()
    free = list(set(all_letters) - set(in_use))
    return free[0]

  output_dir = os.path.join('different-drive', 'output')
  if not os.path.isdir(os.path.abspath(output_dir)):
    os.makedirs(os.path.abspath(output_dir))
  output_drive = GetFirstFreeDriveLetter()
  subprocess.call(['subst', '%c:' % output_drive, os.path.abspath(output_dir)])
  try:
    test.run_gyp('prog.gyp', '--generator-output=%s' % (
        os.path.join(output_drive, 'output')))
    test.build('prog.gyp', test.ALL, chdir=os.path.join(output_drive, 'output'))
    test.built_file_must_exist('program', chdir=os.path.join(output_drive,
                                                             'output'),
                               type=test.EXECUTABLE)
    test.pass_test()
  finally:
    subprocess.call(['subst', '%c:' % output_drive, '/D'])
