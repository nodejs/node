#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that rules which use built dependencies work correctly.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('use-built-dependencies-rule.gyp', chdir='src')

test.relocate('src', 'relocate/src')
test.build('use-built-dependencies-rule.gyp', chdir='relocate/src')

test.built_file_must_exist('main_output', chdir='relocate/src')
test.built_file_must_match('main_output', 'output', chdir='relocate/src')

test.pass_test()
