#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test actions that output to PRODUCT_DIR.
"""

import TestGyp

# TODO fix this for xcode: http://code.google.com/p/gyp/issues/detail?id=88
test = TestGyp.TestGyp(formats=['!xcode'])

test.run_gyp('none.gyp', chdir='src')

test.build('none.gyp', test.ALL, chdir='src')

file_content = 'Hello from make-file.py\n'
subdir_file_content = 'Hello from make-subdir-file.py\n'

test.built_file_must_match('file.out', file_content, chdir='src')
test.built_file_must_match('subdir_file.out', subdir_file_content, chdir='src')

test.pass_test()
