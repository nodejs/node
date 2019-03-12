#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Checks that gyp does not fail on executable targets which have several files
with the same basename.
"""

import TestGyp

# While MSVS supports building executables that contain several files with the
# same name, the msvs gyp generator does not.
test = TestGyp.TestGyp(formats=['!msvs'])

test.run_gyp('double-executable.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('double-executable.gyp', test.ALL, chdir='relocate/src')

expect = """\
Hello from prog3.c
Hello prog3 from func.c
Hello prog3 from subdir1/func.c
Hello prog3 from subdir2/func.c
"""

test.run_built_executable('prog3', chdir='relocate/src', stdout=expect)

test.pass_test()
