#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a scons build picks up tools modules specified
via 'scons_tools' in the 'scons_settings' dictionary.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('tools.gyp')

test.build('tools.gyp', test.ALL)

if test.format == 'scons':
  expect = "Hello, world!\n"
else:
  expect = ""
test.run_built_executable('tools', stdout=expect)

test.pass_test()
