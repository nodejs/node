#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that Makefiles don't get rebuilt when a source gyp file changes and
the disable_regeneration generator flag is set.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('hello.gyp', '-Gauto_regeneration=0')

test.build('hello.gyp', test.ALL)

test.run_built_executable('hello', stdout="Hello, world!\n")

# Sleep so that the changed gyp file will have a newer timestamp than the
# previously generated build files.
test.sleep()
test.write('hello.gyp', test.read('hello2.gyp'))

test.build('hello.gyp', test.ALL)

# Should still be the old executable, as regeneration was disabled.
test.run_built_executable('hello', stdout="Hello, world!\n")

test.pass_test()
