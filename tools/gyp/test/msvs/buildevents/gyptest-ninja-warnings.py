#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ninja errors out when encountering msvs_prebuild/msvs_postbuild.
"""

import sys
import TestCmd
import TestGyp


if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  test.run_gyp('buildevents.gyp',
      status=1,
      stderr=r'.*msvs_prebuild not supported \(target main\).*',
      match=TestCmd.match_re_dotall)

  test.run_gyp('buildevents.gyp',
      status=1,
      stderr=r'.*msvs_postbuild not supported \(target main\).*',
      match=TestCmd.match_re_dotall)

  test.pass_test()
