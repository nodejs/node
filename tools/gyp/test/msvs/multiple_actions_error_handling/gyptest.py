#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that failing actions make the build fail reliably, even when there
are multiple actions in one project.
"""

import os
import sys
import TestGyp
import TestCmd

test = TestGyp.TestGyp(formats=['msvs'], workdir='workarea_all')

test.run_gyp('actions.gyp')
test.build('actions.gyp',
           target='actions-test',
           status=1,
           stdout=r'.*"cmd\.exe" exited with code 1\..*',
           match=TestCmd.match_re_dotall)

test.pass_test()
