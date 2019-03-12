#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that Makefiles get rebuilt when a source gyp file changes and
--generator-output is used.
"""

import TestGyp

# Regenerating build files when a gyp file changes is currently only supported
# by the make generator, and --generator-output is not supported by ninja, so we
# can only test for make.
test = TestGyp.TestGyp(formats=['make'])

CHDIR='generator-output'

test.run_gyp('hello.gyp', '--generator-output=%s' % CHDIR)

test.build('hello.gyp', test.ALL, chdir=CHDIR)

test.run_built_executable('hello', stdout="Hello, world!\n", chdir=CHDIR)

# Sleep so that the changed gyp file will have a newer timestamp than the
# previously generated build files.
test.sleep()
test.write('hello.gyp', test.read('hello2.gyp'))

test.build('hello.gyp', test.ALL, chdir=CHDIR)

test.run_built_executable('hello', stdout="Hello, two!\n", chdir=CHDIR)

test.pass_test()
