#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies inclusion of $HOME/.gyp_new/include.gypi works when GYP_CONFIG_DIR
is set.
"""

import os
import TestGyp

test = TestGyp.TestGyp()

os.environ['HOME'] = os.path.abspath('home')
os.environ['GYP_CONFIG_DIR'] = os.path.join(os.path.abspath('home2'),
                                            '.gyp_new')

test.run_gyp('all.gyp', chdir='src')

# After relocating, we should still be able to build (build file shouldn't
# contain relative reference to ~/.gyp_new/include.gypi)
test.relocate('src', 'relocate/src')

test.build('all.gyp', test.ALL, chdir='relocate/src')

test.run_built_executable('printfoo',
                          chdir='relocate/src',
                          stdout='FOO is fromhome3\n')

test.pass_test()
