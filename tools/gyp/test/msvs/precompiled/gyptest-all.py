#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that precompiled headers can be specified.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['msvs'], workdir='workarea_all')

test.run_gyp('hello.gyp')

test.build('hello.gyp', 'hello')

test.run_built_executable('hello', stdout="Hello, world!\nHello, two!\n")

test.up_to_date('hello.gyp', test.ALL)

test.pass_test()
