#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies inclusion of $HOME/.gyp/include.gypi works when --config-dir is
specified.
"""

import os
import TestGyp

test = TestGyp.TestGyp()

os.environ['HOME'] = os.path.abspath('home2')

test.run_gyp('all.gyp', '--config-dir=~/.gyp_new', chdir='src')

# After relocating, we should still be able to build (build file shouldn't
# contain relative reference to ~/.gyp/include.gypi)
test.relocate('src', 'relocate/src')

test.build('all.gyp', test.ALL, chdir='relocate/src')

test.run_built_executable('printfoo',
                          chdir='relocate/src',
                          stdout='FOO is fromhome3\n')

test.pass_test()
