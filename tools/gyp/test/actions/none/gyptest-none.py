#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies actions can be in 'none' type targets with source files.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('none_with_source_files.gyp', chdir='src')

test.relocate('src', 'relocate/src')
test.build('none_with_source_files.gyp', chdir='relocate/src')

file_content = 'foo.cc\n'

test.built_file_must_match('fake.out', file_content, chdir='relocate/src')

test.pass_test()
