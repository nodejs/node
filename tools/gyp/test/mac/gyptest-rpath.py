#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that LD_DYLIB_INSTALL_NAME and DYLIB_INSTALL_NAME_BASE are handled
correctly.
"""

import TestGyp

import re
import subprocess
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'rpath'
  test.run_gyp('test.gyp', chdir=CHDIR)

  test.build('test.gyp', test.ALL, chdir=CHDIR)

  def GetRpaths(p):
    p = test.built_file_path(p, chdir=CHDIR)
    r = re.compile(r'cmd LC_RPATH.*?path (.*?) \(offset \d+\)', re.DOTALL)
    proc = subprocess.Popen(['otool', '-l', p], stdout=subprocess.PIPE)
    o = proc.communicate()[0].decode('utf-8')
    assert not proc.returncode
    return r.findall(o)

  if GetRpaths('libdefault_rpath.dylib') != []:
    test.fail_test()

  if GetRpaths('libexplicit_rpath.dylib') != ['@executable_path/.']:
    test.fail_test()

  if (GetRpaths('libexplicit_rpaths_escaped.dylib') !=
      ['First rpath', 'Second rpath']):
    test.fail_test()

  if GetRpaths('My Framework.framework/My Framework') != ['@loader_path/.']:
    test.fail_test()

  if GetRpaths('executable') != ['@executable_path/.']:
    test.fail_test()

  test.pass_test()
