#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that xcode-style GCC_... settings are handled properly.
"""

import TestGyp

import subprocess
import sys

def CompilerVersion(compiler):
  stdout = subprocess.check_output([compiler, '-v'], stderr=subprocess.STDOUT)
  stdout = stdout.decode('utf-8')
  return stdout.rstrip('\n')


if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  if test.format == 'xcode-ninja':
    test.skip_test()

  CHDIR = 'xcode-gcc'
  test.run_gyp('test.gyp', chdir=CHDIR)

  # List of targets that'll pass. It expects targets of the same name with
  # '-fail' appended that'll fail to build.
  targets = [
    'warn_about_missing_newline',
  ]

  # "clang" does not support the "-Winvalid-offsetof" flag, and silently
  # ignore it. Starting with Xcode 5.0.0, "gcc" is just a "clang" binary with
  # some hard-coded include path hack, so use the output of "-v" to detect if
  # the compiler supports the flag or not.
  if 'clang' not in CompilerVersion('/usr/bin/cc'):
    targets.append('warn_about_invalid_offsetof_macro')

  for target in targets:
    test.build('test.gyp', target, chdir=CHDIR)
    test.built_file_must_exist(target, chdir=CHDIR)
    fail_target = target + '-fail'
    test.build('test.gyp', fail_target, chdir=CHDIR, status=None, stderr=None, match=lambda *args: True)
    test.built_file_must_not_exist(fail_target, chdir=CHDIR)

  test.pass_test()
