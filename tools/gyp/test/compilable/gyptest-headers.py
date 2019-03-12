#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that .hpp files are ignored when included in the source list on all
platforms.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('headers.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('headers.gyp', test.ALL, chdir='relocate/src')

expect = """\
Hello from program.c
Hello from lib1.c
"""
test.run_built_executable('program', chdir='relocate/src', stdout=expect)


test.pass_test()
