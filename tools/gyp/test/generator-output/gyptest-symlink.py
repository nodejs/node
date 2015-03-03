#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies building a target when the --generator-output= option is used to put
the build configuration files in a separate directory tree referenced by a
symlink.
"""

import TestGyp
import os

# Android doesn't support --generator-output.
test = TestGyp.TestGyp(formats=['!android'])
if not hasattr(os, 'symlink'):
  test.skip_test('Missing os.symlink -- skipping test.\n')

test.writable(test.workpath('src'), False)

test.writable(test.workpath('src/subdir2/deeper/build'), True)

test.subdir(test.workpath('build'))
test.subdir(test.workpath('build/deeper'))
test.symlink('build/deeper', test.workpath('symlink'))

test.writable(test.workpath('build/deeper'), True)
test.run_gyp('deeper.gyp',
             '-Dset_symroot=2',
             '--generator-output=' + test.workpath('symlink'),
             chdir='src/subdir2/deeper')

chdir = 'symlink'
test.build('deeper.gyp', test.ALL, chdir=chdir)

if test.format == 'xcode':
  chdir = 'src/subdir2/deeper'
test.run_built_executable('deeper',
                          chdir=chdir,
                          stdout="Hello from deeper.c\n")
test.pass_test()
