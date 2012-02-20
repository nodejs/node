#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies building a subsidiary dependent target from a .gyp file in a
subdirectory, without specifying an explicit output build directory,
and using the subdirectory's solution or project file as the entry point.
"""

import TestGyp
import errno

test = TestGyp.TestGyp(formats=['ninja', 'make'])

# We want our Makefile to be one dir up from main.gyp.
test.run_gyp('main.gyp', '--toplevel-dir=..', chdir='src/sub1')

toplevel_dir = 'src'

test.build('all', chdir=toplevel_dir)

test.built_file_must_exist('prog1', type=test.EXECUTABLE, chdir=toplevel_dir)

test.run_built_executable('prog1',
                          chdir=toplevel_dir,
                          stdout="Hello from prog1.c\n")

test.pass_test()
