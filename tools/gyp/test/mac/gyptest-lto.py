#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that LTO flags work.
"""

import TestGyp

import os
import re
import subprocess
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'lto'
  test.run_gyp('test.gyp', chdir=CHDIR)

  test.build('test.gyp', test.ALL, chdir=CHDIR)

  def ObjPath(srcpath, target):
    # TODO: Move this into TestGyp if it's needed elsewhere.
    if test.format == 'xcode':
      return os.path.join(CHDIR, 'build', 'test.build', 'Default',
                          target + '.build', 'Objects-normal', 'x86_64',
                          srcpath + '.o')
    elif 'ninja' in test.format:  # ninja, xcode-ninja
      return os.path.join(CHDIR, 'out', 'Default', 'obj',
                          target + '.' + srcpath + '.o')
    elif test.format == 'make':
      return os.path.join(CHDIR, 'out', 'Default', 'obj.target',
                          target, srcpath + '.o')

  def ObjType(p, t_expected):
    # r = re.compile(r'nsyms\s+(\d+)')
    o = subprocess.check_output(['file', p]).decode('utf-8')
    objtype = 'unknown'
    if ': Mach-O ' in o:
      objtype = 'mach-o'
    elif ': LLVM bit' in o:
      objtype = 'llvm'
    if objtype != t_expected:
      print('Expected %s, got %s' % (t_expected, objtype))
      test.fail_test()

  ObjType(ObjPath('cfile', 'lto'), 'llvm')
  ObjType(ObjPath('ccfile', 'lto'), 'llvm')
  ObjType(ObjPath('mfile', 'lto'), 'llvm')
  ObjType(ObjPath('mmfile', 'lto'), 'llvm')
  ObjType(ObjPath('asmfile', 'lto'), 'mach-o')

  ObjType(ObjPath('cfile', 'lto_static'), 'llvm')
  ObjType(ObjPath('ccfile', 'lto_static'), 'llvm')
  ObjType(ObjPath('mfile', 'lto_static'), 'llvm')
  ObjType(ObjPath('mmfile', 'lto_static'), 'llvm')
  ObjType(ObjPath('asmfile', 'lto_static'), 'mach-o')

  test.pass_test()

  # TODO: Probably test for -object_path_lto too, else dsymutil won't be
  # useful maybe?
