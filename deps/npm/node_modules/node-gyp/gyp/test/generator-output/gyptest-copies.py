#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies file copies with --generator-output using an explicit build
target of 'all'.
"""

import TestGyp

# Ninja and Android don't support --generator-output.
test = TestGyp.TestGyp(formats=['!ninja', '!android'])

test.writable(test.workpath('copies'), False)

test.run_gyp('copies.gyp',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='copies')

test.writable(test.workpath('copies'), True)

test.relocate('copies', 'relocate/copies')
test.relocate('gypfiles', 'relocate/gypfiles')

test.writable(test.workpath('relocate/copies'), False)

test.writable(test.workpath('relocate/copies/build'), True)
test.writable(test.workpath('relocate/copies/copies-out'), True)
test.writable(test.workpath('relocate/copies/subdir/build'), True)
test.writable(test.workpath('relocate/copies/subdir/copies-out'), True)

test.build('copies.gyp', test.ALL, chdir='relocate/gypfiles')

test.must_match(['relocate', 'copies', 'copies-out', 'file1'],
                "file1 contents\n")

if test.format == 'xcode':
  chdir = 'relocate/copies/build'
elif test.format == 'make':
  chdir = 'relocate/gypfiles/out'
else:
  chdir = 'relocate/gypfiles'
test.must_match([chdir, 'Default', 'copies-out', 'file2'], "file2 contents\n")

test.must_match(['relocate', 'copies', 'subdir', 'copies-out', 'file3'],
                "file3 contents\n")

if test.format == 'xcode':
  chdir = 'relocate/copies/subdir/build'
elif test.format == 'make':
  chdir = 'relocate/gypfiles/out'
else:
  chdir = 'relocate/gypfiles'
test.must_match([chdir, 'Default', 'copies-out', 'file4'], "file4 contents\n")

test.pass_test()
