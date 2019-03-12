#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies building a project rooted several layers under src_dir works.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('prog3.gyp', chdir='src/subdir/subdir2')

test.relocate('src', 'relocate/src')

test.build('prog3.gyp', test.ALL, chdir='relocate/src/subdir/subdir2')

test.run_built_executable('prog3',
                          chdir='relocate/src/subdir/subdir2',
                          stdout="Hello from prog3.c\n")

test.pass_test()
