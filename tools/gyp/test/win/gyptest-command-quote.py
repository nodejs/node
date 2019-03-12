#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""

Make sure the program in a command can be a called batch file, or an
application in the path. Specifically, this means not quoting something like
"call x.bat", lest the shell look for a program named "call x.bat", rather
than calling "x.bat".
"""

from __future__ import print_function

import TestGyp

import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])
  CHDIR = 'command-quote'
  test.run_gyp('command-quote.gyp', chdir=CHDIR)

  test.build('command-quote.gyp', 'test_batch', chdir=CHDIR)
  test.build('command-quote.gyp', 'test_call_separate', chdir=CHDIR)
  test.build('command-quote.gyp', 'test_with_double_quotes', chdir=CHDIR)
  test.build('command-quote.gyp', 'test_with_single_quotes', chdir=CHDIR)

  # We confirm that this fails because other generators don't handle spaces in
  # inputs so it's preferable to not have it work here.
  test.build('command-quote.gyp', 'test_with_spaces', chdir=CHDIR, status=1)

  CHDIR = 'command-quote/subdir/and/another'
  test.run_gyp('in-subdir.gyp', chdir=CHDIR)
  test.build('in-subdir.gyp', 'test_batch_depth', chdir=CHDIR)

  test.pass_test()
