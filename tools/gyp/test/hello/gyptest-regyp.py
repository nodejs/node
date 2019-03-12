#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that Makefiles get rebuilt when a source gyp file changes.
"""

import TestGyp

# Regenerating build files when a gyp file changes is currently only supported
# by the make generator.
test = TestGyp.TestGyp(formats=['make'])

test.run_gyp('hello.gyp')

test.build('hello.gyp', test.ALL)

test.run_built_executable('hello', stdout="Hello, world!\n")

# Sleep so that the changed gyp file will have a newer timestamp than the
# previously generated build files.
test.sleep()
test.write('hello.gyp', test.read('hello2.gyp'))

test.build('hello.gyp', test.ALL)

test.run_built_executable('hello', stdout="Hello, two!\n")

test.pass_test()
