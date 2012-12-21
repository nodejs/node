#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simplest-possible build of a "Hello, world!" program
using the default build target.
"""

import TestGyp

test = TestGyp.TestGyp(workdir='workarea_default')

test.run_gyp('hello.gyp')

test.build('hello.gyp')

test.run_built_executable('hello', stdout="Hello, world!\n")

test.up_to_date('hello.gyp', test.DEFAULT)

test.pass_test()
