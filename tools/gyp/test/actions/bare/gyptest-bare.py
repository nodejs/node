#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies actions which are not depended on by other targets get executed.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('bare.gyp', chdir='src')

test.relocate('src', 'relocate/src')
test.build('bare.gyp', chdir='relocate/src')

file_content = 'Hello from bare.py\n'

test.built_file_must_match('out.txt', file_content, chdir='relocate/src')

test.pass_test()
