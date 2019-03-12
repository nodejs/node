#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure buffer security check setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('buffer-security-check.gyp', chdir=CHDIR)
  test.build('buffer-security-check.gyp', chdir=CHDIR)

  def GetDisassemblyOfMain(exe):
    # The standard library uses buffer security checks independent of our
    # buffer security settings, so we extract just our code (i.e. main()) to
    # check against.
    full_path = test.built_file_path(exe, chdir=CHDIR)
    output = test.run_dumpbin('/disasm', full_path)
    result = []
    in_main = False
    for line in output.splitlines():
      if line == '_main:':
        in_main = True
      elif in_main:
        # Disassembly of next function starts.
        if line.startswith('_'):
          break
        result.append(line)
    return '\n'.join(result)

  # Buffer security checks are on by default, make sure security_cookie
  # appears in the disassembly of our code.
  if 'security_cookie' not in GetDisassemblyOfMain('test_bsc_unset.exe'):
    test.fail_test()

  # Explicitly on.
  if 'security_cookie' not in GetDisassemblyOfMain('test_bsc_on.exe'):
    test.fail_test()

  # Explicitly off, shouldn't be a reference to the security cookie.
  if 'security_cookie' in GetDisassemblyOfMain('test_bsc_off.exe'):
    test.fail_test()

  test.pass_test()
