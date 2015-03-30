#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that disabling target aliases works.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['android'])

test.run_gyp('hello.gyp', '-G', 'write_alias_targets=0')

test.build('hello.gyp', 'hello', status=2, stderr=None)

test.build('hello.gyp', 'gyp_all_modules', status=2, stderr=None)

test.pass_test()
