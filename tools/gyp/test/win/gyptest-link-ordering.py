#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure the link order of object files is the same between msvs and ninja.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('link-ordering.gyp', chdir=CHDIR)
  test.build('link-ordering.gyp', test.ALL, chdir=CHDIR)

  def GetDisasm(exe):
    full_path = test.built_file_path(exe, chdir=CHDIR)
    # Get disassembly and drop int3 padding between functions.
    return '\n'.join(
        x for x in test.run_dumpbin('/disasm', full_path).splitlines()
                   if 'CC' not in x)

  # This is the full dump that we expect. The source files in the .gyp match
  # this order which is what determines the ordering in the binary.

  expected_disasm_basic = '''
_mainCRTStartup:
  00401000: B8 05 00 00 00     mov         eax,5
  00401005: C3                 ret
?z@@YAHXZ:
  00401010: B8 03 00 00 00     mov         eax,3
  00401015: C3                 ret
?x@@YAHXZ:
  00401020: B8 01 00 00 00     mov         eax,1
  00401025: C3                 ret
?y@@YAHXZ:
  00401030: B8 02 00 00 00     mov         eax,2
  00401035: C3                 ret
_main:
  00401040: 33 C0              xor         eax,eax
  00401042: C3                 ret
'''

  if expected_disasm_basic not in GetDisasm('test_ordering_exe.exe'):
    print GetDisasm('test_ordering_exe.exe')
    test.fail_test()

  # Similar to above. The VS generator handles subdirectories differently.

  expected_disasm_subdirs = '''
_mainCRTStartup:
  00401000: B8 05 00 00 00     mov         eax,5
  00401005: C3                 ret
_main:
  00401010: 33 C0              xor         eax,eax
  00401012: C3                 ret
?y@@YAHXZ:
  00401020: B8 02 00 00 00     mov         eax,2
  00401025: C3                 ret
?z@@YAHXZ:
  00401030: B8 03 00 00 00     mov         eax,3
  00401035: C3                 ret
'''

  if expected_disasm_subdirs not in GetDisasm('test_ordering_subdirs.exe'):
    print GetDisasm('test_ordering_subdirs.exe')
    test.fail_test()

  # Similar, but with directories mixed into folders (crt and main at the same
  # level, but with a subdir in the middle).

  expected_disasm_subdirs_mixed = '''
_mainCRTStartup:
  00401000: B8 05 00 00 00     mov         eax,5
  00401005: C3                 ret
?x@@YAHXZ:
  00401010: B8 01 00 00 00     mov         eax,1
  00401015: C3                 ret
_main:
  00401020: 33 C0              xor         eax,eax
  00401022: C3                 ret
?z@@YAHXZ:
  00401030: B8 03 00 00 00     mov         eax,3
  00401035: C3                 ret
?y@@YAHXZ:
  00401040: B8 02 00 00 00     mov         eax,2
  00401045: C3                 ret
'''

  if (expected_disasm_subdirs_mixed not in
      GetDisasm('test_ordering_subdirs_mixed.exe')):
    print GetDisasm('test_ordering_subdirs_mixed.exe')
    test.fail_test()

  test.pass_test()
