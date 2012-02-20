#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies inclusion of $HOME/.gyp/include.gypi works properly with relocation
and with regeneration.
"""

import os
import TestGyp

# Regenerating build files when a gyp file changes is currently only supported
# by the make generator.
test = TestGyp.TestGyp(formats=['make'])

os.environ['HOME'] = os.path.abspath('home')

test.run_gyp('all.gyp', chdir='src')

# After relocating, we should still be able to build (build file shouldn't
# contain relative reference to ~/.gyp/include.gypi)
test.relocate('src', 'relocate/src')

test.build('all.gyp', test.ALL, chdir='relocate/src')

test.run_built_executable('printfoo',
                          chdir='relocate/src',
                          stdout='FOO is fromhome\n')

# Building should notice any changes to ~/.gyp/include.gypi and regyp.
test.sleep()

test.write('home/.gyp/include.gypi', test.read('home2/.gyp/include.gypi'))

test.build('all.gyp', test.ALL, chdir='relocate/src')

test.run_built_executable('printfoo',
                          chdir='relocate/src',
                          stdout='FOO is fromhome2\n')

test.pass_test()
