#!/usr/bin/env python

# Copyright 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure the base address setting is extracted properly.
"""

import TestGyp

import re
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('base-address.gyp', chdir=CHDIR)
  test.build('base-address.gyp', test.ALL, chdir=CHDIR)

  def GetHeaders(exe):
    full_path = test.built_file_path(exe, chdir=CHDIR)
    return test.run_dumpbin('/headers', full_path)

  # Extract the image base address from the headers output.
  image_base_reg_ex = re.compile(r'.*\s+([0-9]+) image base.*', re.DOTALL)

  exe_headers = GetHeaders('test_base_specified_exe.exe')
  exe_match = image_base_reg_ex.match(exe_headers)

  if not exe_match or not exe_match.group(1):
    test.fail_test()
  if exe_match.group(1) != '420000':
    test.fail_test()

  dll_headers = GetHeaders('test_base_specified_dll.dll')
  dll_match = image_base_reg_ex.match(dll_headers)

  if not dll_match or not dll_match.group(1):
    test.fail_test()
  if dll_match.group(1) != '10420000':
    test.fail_test()

  default_exe_headers = GetHeaders('test_base_default_exe.exe')
  default_exe_match = image_base_reg_ex.match(default_exe_headers)

  if not default_exe_match or not default_exe_match.group(1):
    test.fail_test()
  if default_exe_match.group(1) != '400000':
    test.fail_test()

  default_dll_headers = GetHeaders('test_base_default_dll.dll')
  default_dll_match = image_base_reg_ex.match(default_dll_headers)

  if not default_dll_match or not default_dll_match.group(1):
    test.fail_test()
  if default_dll_match.group(1) != '10000000':
    test.fail_test()

  test.pass_test()
