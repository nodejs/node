#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that the implicit rpath is added only when needed.
"""

import TestGyp

import re
import subprocess
import sys

if sys.platform.startswith('linux'):
  test = TestGyp.TestGyp(formats=['ninja', 'make'])

  CHDIR = 'implicit-rpath'
  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', test.ALL, chdir=CHDIR)

  def GetRpaths(p):
    p = test.built_file_path(p, chdir=CHDIR)
    r = re.compile(r'Library rpath: \[([^\]]+)\]')
    proc = subprocess.Popen(['readelf', '-d', p], stdout=subprocess.PIPE)
    o = proc.communicate()[0]
    assert not proc.returncode
    return r.findall(o)

  if test.format == 'ninja':
    expect = '$ORIGIN/lib/'
  elif test.format == 'make':
    expect = '$ORIGIN/lib.target/'
  else:
    test.fail_test()

  if GetRpaths('shared_executable') != [expect]:
    test.fail_test()

  if GetRpaths('shared_executable_no_so_suffix') != [expect]:
    test.fail_test()

  if GetRpaths('static_executable'):
    test.fail_test()

  test.pass_test()
