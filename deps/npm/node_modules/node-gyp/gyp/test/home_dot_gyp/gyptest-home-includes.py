#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies inclusion of $HOME/.gyp/include.gypi works.
"""

import os
import TestGyp

test = TestGyp.TestGyp()

os.environ['HOME'] = os.path.abspath('home')

test.run_gyp('all.gyp', chdir='src')

# After relocating, we should still be able to build (build file shouldn't
# contain relative reference to ~/.gyp/include.gypi)
test.relocate('src', 'relocate/src')

test.build('all.gyp', test.ALL, chdir='relocate/src')

test.run_built_executable('printfoo',
                          chdir='relocate/src',
                          stdout='FOO is fromhome\n')

test.pass_test()
