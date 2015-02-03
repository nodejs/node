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

  CHDIR = 'installname'
  test.run_gyp('test.gyp', chdir=CHDIR)

  test.build('test.gyp', test.ALL, chdir=CHDIR)

  def GetInstallname(p):
    p = test.built_file_path(p, chdir=CHDIR)
    r = re.compile(r'cmd LC_ID_DYLIB.*?name (.*?) \(offset \d+\)', re.DOTALL)
    proc = subprocess.Popen(['otool', '-l', p], stdout=subprocess.PIPE)
    o = proc.communicate()[0]
    assert not proc.returncode
    m = r.search(o)
    assert m
    return m.group(1)

  if (GetInstallname('libdefault_installname.dylib') !=
      '/usr/local/lib/libdefault_installname.dylib'):
    test.fail_test()

  if (GetInstallname('My Framework.framework/My Framework') !=
      '/Library/Frameworks/My Framework.framework/'
      'Versions/A/My Framework'):
    test.fail_test()

  if (GetInstallname('libexplicit_installname.dylib') !=
      'Trapped in a dynamiclib factory'):
    test.fail_test()

  if (GetInstallname('libexplicit_installname_base.dylib') !=
      '@executable_path/../../../libexplicit_installname_base.dylib'):
    test.fail_test()

  if (GetInstallname('My Other Framework.framework/My Other Framework') !=
      '@executable_path/../../../My Other Framework.framework/'
      'Versions/A/My Other Framework'):
    test.fail_test()

  if (GetInstallname('libexplicit_installname_with_base.dylib') !=
      '/usr/local/lib/libexplicit_installname_with_base.dylib'):
    test.fail_test()

  if (GetInstallname('libexplicit_installname_with_explicit_base.dylib') !=
      '@executable_path/../libexplicit_installname_with_explicit_base.dylib'):
    test.fail_test()

  if (GetInstallname('libboth_base_and_installname.dylib') !=
      'Still trapped in a dynamiclib factory'):
    test.fail_test()

  if (GetInstallname('install_name_with_info_plist.framework/'
                     'install_name_with_info_plist') !=
      '/Library/Frameworks/install_name_with_info_plist.framework/'
      'Versions/A/install_name_with_info_plist'):
    test.fail_test()

  if ('DYLIB_INSTALL_NAME_BASE:standardizepath: command not found' in
          test.stdout()):
    test.fail_test()

  test.pass_test()
