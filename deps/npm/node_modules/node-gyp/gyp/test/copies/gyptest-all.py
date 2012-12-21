#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies file copies using an explicit build target of 'all'.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('copies.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('copies.gyp', test.ALL, chdir='relocate/src')

test.must_match(['relocate', 'src', 'copies-out', 'file1'], 'file1 contents\n')

test.built_file_must_match('copies-out/file2',
                           'file2 contents\n',
                           chdir='relocate/src')

test.built_file_must_match('copies-out/directory/file3',
                           'file3 contents\n',
                           chdir='relocate/src')
test.built_file_must_match('copies-out/directory/file4',
                           'file4 contents\n',
                           chdir='relocate/src')
test.built_file_must_match('copies-out/directory/subdir/file5',
                           'file5 contents\n',
                           chdir='relocate/src')
test.built_file_must_match('copies-out/subdir/file6',
                           'file6 contents\n',
                           chdir='relocate/src')

test.pass_test()
