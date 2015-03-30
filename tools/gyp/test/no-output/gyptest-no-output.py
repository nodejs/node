#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verified things don't explode when there are targets without outputs.
"""

import TestGyp

# TODO(evan): in ninja when there are no targets, there is no 'all'
# target either.  Disabling this test for now.
test = TestGyp.TestGyp(formats=['!ninja'])

test.run_gyp('nooutput.gyp', chdir='src')
test.relocate('src', 'relocate/src')
test.build('nooutput.gyp', chdir='relocate/src')

test.pass_test()
