#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simplest-possible build of a "Hello, world!" program
using an explicit build target of 'hello'.
"""

import TestGyp

test = TestGyp.TestGyp(workdir='workarea_target')

test.run_gyp('hello.gyp')

test.build('hello.gyp', 'hello')

test.run_built_executable('hello', stdout="Hello, world!\n")

test.up_to_date('hello.gyp', 'hello')

test.pass_test()
