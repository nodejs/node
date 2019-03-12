#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure StackReserveSize and StackCommitSize settings are extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('stacksize.gyp', chdir=CHDIR)
  test.build('stacksize.gyp', test.ALL, chdir=CHDIR)

  def GetHeaders(exe):
    return test.run_dumpbin('/headers', test.built_file_path(exe, chdir=CHDIR))

  # Verify default sizes as reported by dumpbin:
  #     100000h = 1MB
  #     1000h   = 4KB
  default_headers = GetHeaders('test_default.exe')
  if '100000 size of stack reserve' not in default_headers:
    test.fail_test()
  if '1000 size of stack commit' not in default_headers:
    test.fail_test()

  # Verify that reserved size is changed, but commit size is unchanged:
  #     200000h = 2MB
  #     1000h   = 4KB
  set_reserved_size_headers = GetHeaders('test_set_reserved_size.exe')
  if '200000 size of stack reserve' not in set_reserved_size_headers:
    test.fail_test()
  if '1000 size of stack commit' not in set_reserved_size_headers:
    test.fail_test()

  # Verify that setting the commit size, without the reserve size, has no
  # effect:
  #     100000h = 1MB
  #     1000h   = 4KB
  set_commit_size_headers = GetHeaders('test_set_commit_size.exe')
  if '100000 size of stack reserve' not in set_commit_size_headers:
    test.fail_test()
  if '1000 size of stack commit' not in set_commit_size_headers:
    test.fail_test()

  # Verify that setting both works:
  #     200000h = 2MB
  #     2000h   = 8KB
  set_both_headers = GetHeaders('test_set_both.exe')
  if '200000 size of stack reserve' not in set_both_headers:
    test.fail_test()
  if '2000 size of stack commit' not in set_both_headers:
    test.fail_test()

  test.pass_test()
