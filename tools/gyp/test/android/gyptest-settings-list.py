#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that it's possible to set Android build system settings using the
aosp_build_settings key, with list values.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['android'])

test.run_gyp('settings-list.gyp')

test.build('settings-list.gyp', 'hello')

test.run_built_executable('hello.foo', stdout="Hello, world!\n")

test.up_to_date('settings-list.gyp', test.DEFAULT)

test.pass_test()
