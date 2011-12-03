#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that .hpp files are ignored when included in the source list on all
platforms.
"""

import sys
import TestGyp

# TODO(bradnelson): get this working for windows.
test = TestGyp.TestGyp(formats=['make', 'ninja', 'scons', 'xcode'])

test.run_gyp('assembly.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('assembly.gyp', test.ALL, chdir='relocate/src')

expect = """\
Hello from program.c
Got 42.
"""
test.run_built_executable('program', chdir='relocate/src', stdout=expect)


test.pass_test()
