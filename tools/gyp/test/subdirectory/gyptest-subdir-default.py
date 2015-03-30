#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies building a subsidiary dependent target from a .gyp file in a
subdirectory, without specifying an explicit output build directory,
and using the subdirectory's solution or project file as the entry point.
"""

import TestGyp
import errno

# Android doesn't support running from subdirectories.
# Ninja doesn't support relocation.
# CMake produces a single CMakeLists.txt in the output directory.
test = TestGyp.TestGyp(formats=['!ninja', '!android', '!cmake'])

test.run_gyp('prog1.gyp', chdir='src')

test.relocate('src', 'relocate/src')

chdir = 'relocate/src/subdir'

test.build('prog2.gyp', chdir=chdir)

test.built_file_must_not_exist('prog1', type=test.EXECUTABLE, chdir=chdir)

test.run_built_executable('prog2',
                          chdir=chdir,
                          stdout="Hello from prog2.c\n")

test.pass_test()
