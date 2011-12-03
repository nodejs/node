#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies --generator-output= behavior when using rules.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['!ninja'])

test.writable(test.workpath('rules'), False)

test.run_gyp('rules.gyp',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='rules')

test.writable(test.workpath('rules'), True)

test.relocate('rules', 'relocate/rules')
test.relocate('gypfiles', 'relocate/gypfiles')

test.writable(test.workpath('relocate/rules'), False)

test.writable(test.workpath('relocate/rules/build'), True)
test.writable(test.workpath('relocate/rules/subdir1/build'), True)
test.writable(test.workpath('relocate/rules/subdir2/build'), True)
test.writable(test.workpath('relocate/rules/subdir2/rules-out'), True)

test.build('rules.gyp', test.ALL, chdir='relocate/gypfiles')

expect = """\
Hello from program.c
Hello from function1.in1
Hello from function2.in1
Hello from define3.in0
Hello from define4.in0
"""

if test.format == 'xcode':
  chdir = 'relocate/rules/subdir1'
else:
  chdir = 'relocate/gypfiles'
test.run_built_executable('program', chdir=chdir, stdout=expect)

test.must_match('relocate/rules/subdir2/rules-out/file1.out',
                "Hello from file1.in0\n")
test.must_match('relocate/rules/subdir2/rules-out/file2.out',
                "Hello from file2.in0\n")
test.must_match('relocate/rules/subdir2/rules-out/file3.out',
                "Hello from file3.in1\n")
test.must_match('relocate/rules/subdir2/rules-out/file4.out',
                "Hello from file4.in1\n")

test.pass_test()
