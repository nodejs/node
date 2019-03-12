#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies file copies with a trailing slash in the destination directory.
"""

import TestGyp

test = TestGyp.TestGyp()
test.run_gyp('copies-slash.gyp', chdir='src')

test.relocate('src', 'relocate/src')
test.build('copies-slash.gyp', chdir='relocate/src')

test.built_file_must_match('copies-out-slash/directory/file3',
                           'file3 contents\n',
                           chdir='relocate/src')
test.built_file_must_match('copies-out-slash/directory/file4',
                           'file4 contents\n',
                           chdir='relocate/src')
test.built_file_must_match('copies-out-slash/directory/subdir/file5',
                           'file5 contents\n',
                           chdir='relocate/src')

test.built_file_must_match('copies-out-slash-2/directory/file3',
                           'file3 contents\n',
                           chdir='relocate/src')
test.built_file_must_match('copies-out-slash-2/directory/file4',
                           'file4 contents\n',
                           chdir='relocate/src')
test.built_file_must_match('copies-out-slash-2/directory/subdir/file5',
                           'file5 contents\n',
                           chdir='relocate/src')

test.pass_test()
