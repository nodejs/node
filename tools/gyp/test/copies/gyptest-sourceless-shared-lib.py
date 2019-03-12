#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies copies in sourceless shared_library targets are executed.
"""

import TestGyp

test = TestGyp.TestGyp()
test.run_gyp('copies-sourceless-shared-lib.gyp', chdir='src')
test.relocate('src', 'relocate/src')
test.build('copies-sourceless-shared-lib.gyp', chdir='relocate/src')
test.built_file_must_match('copies-out/file1',
                           'file1 contents\n',
                           chdir='relocate/src')
test.pass_test()
