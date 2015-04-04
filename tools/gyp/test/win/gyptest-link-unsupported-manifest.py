#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure we error out if #pragma comments are used to modify manifests.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  # This assertion only applies to the ninja build.
  test = TestGyp.TestGyp(formats=['ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('unsupported-manifest.gyp', chdir=CHDIR)

  # Just needs to fail to build.
  test.build('unsupported-manifest.gyp',
      'test_unsupported', chdir=CHDIR, status=1)
  test.must_not_exist(test.built_file_path('test_unsupported.exe', chdir=CHDIR))

  test.pass_test()
