#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that copying files preserves file attributes.
"""

import TestGyp

import os
import stat
import sys


def check_attribs(path, expected_exec_bit):
  out_path = test.built_file_path(path, chdir='src')

  in_stat = os.stat(os.path.join('src', path))
  out_stat = os.stat(out_path)
  if out_stat.st_mode & stat.S_IXUSR != expected_exec_bit:
    test.fail_test()


test = TestGyp.TestGyp()

test.run_gyp('copies-attribs.gyp', chdir='src')

test.build('copies-attribs.gyp', chdir='src')

if sys.platform != 'win32':
  out_path = test.built_file_path('executable-file.sh', chdir='src')
  test.must_contain(out_path,
                    '#!/bin/bash\n'
                    '\n'
                    'echo echo echo echo cho ho o o\n')
  check_attribs('executable-file.sh', expected_exec_bit=stat.S_IXUSR)

test.pass_test()
