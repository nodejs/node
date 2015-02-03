#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Checks that files whose file case changes get rebuilt correctly.
"""

import os
import TestGyp

test = TestGyp.TestGyp()
CHDIR = 'filecase'
test.run_gyp('test.gyp', chdir=CHDIR)
test.build('test.gyp', test.ALL, chdir=CHDIR)

os.rename('filecase/file.c', 'filecase/fIlE.c')
test.write('filecase/test.gyp',
           test.read('filecase/test.gyp').replace('file.c', 'fIlE.c'))
test.run_gyp('test.gyp', chdir=CHDIR)
test.build('test.gyp', test.ALL, chdir=CHDIR)


# Check that having files that differ just in their case still work on
# case-sensitive file systems.
test.write('filecase/FiLe.c', 'int f(); int main() { return f(); }')
test.write('filecase/fIlE.c', 'int f() { return 42; }')
is_case_sensitive = test.read('filecase/FiLe.c') != test.read('filecase/fIlE.c')
if is_case_sensitive:
  test.run_gyp('test-casesensitive.gyp', chdir=CHDIR)
  test.build('test-casesensitive.gyp', test.ALL, chdir=CHDIR)

test.pass_test()
