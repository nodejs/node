#!/usr/bin/env python

# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies building a project hierarchy created when the --generator-output=
and --depth= options is used to put the build configuration files in a separate
directory tree.
"""

import TestGyp
import os

# This is a regression test for the make generator only.
test = TestGyp.TestGyp(formats=['make'])

test.writable(test.workpath('src'), False)

toplevel_dir = os.path.basename(test.workpath())

test.run_gyp(os.path.join(toplevel_dir, 'src', 'prog1.gyp'),
             '-Dset_symroot=1',
             '--generator-output=gypfiles',
             depth=toplevel_dir,
             chdir='..')

test.writable(test.workpath('src/build'), True)
test.writable(test.workpath('src/subdir2/build'), True)
test.writable(test.workpath('src/subdir3/build'), True)

test.build('prog1.gyp', test.ALL, chdir='gypfiles')

chdir = 'gypfiles'

expect = """\
Hello from %s
Hello from inc.h
Hello from inc1/include1.h
Hello from inc2/include2.h
Hello from inc3/include3.h
Hello from subdir2/deeper/deeper.h
"""

if test.format == 'xcode':
  chdir = 'src'
test.run_built_executable('prog1', chdir=chdir, stdout=expect % 'prog1.c')

if test.format == 'xcode':
  chdir = 'src/subdir2'
test.run_built_executable('prog2', chdir=chdir, stdout=expect % 'prog2.c')

if test.format == 'xcode':
  chdir = 'src/subdir3'
test.run_built_executable('prog3', chdir=chdir, stdout=expect % 'prog3.c')

test.pass_test()
