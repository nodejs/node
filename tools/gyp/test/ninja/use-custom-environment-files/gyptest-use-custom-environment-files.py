#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure environment files can be suppressed.
"""

import TestGyp

import os
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  test.run_gyp('use-custom-environment-files.gyp',
               '-G', 'ninja_use_custom_environment_files')

  # Make sure environment files do not exist.
  if os.path.exists(test.built_file_path('environment.x86')):
    test.fail_test()
  if os.path.exists(test.built_file_path('environment.x64')):
    test.fail_test()

  test.pass_test()
