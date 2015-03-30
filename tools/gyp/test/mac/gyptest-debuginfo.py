#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests things related to debug information generation.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='debuginfo')

  test.build('test.gyp', test.ALL, chdir='debuginfo')

  test.built_file_must_exist('libnonbundle_shared_library.dylib.dSYM',
                             chdir='debuginfo')
  test.built_file_must_exist('nonbundle_loadable_module.so.dSYM',
                             chdir='debuginfo')
  test.built_file_must_exist('nonbundle_executable.dSYM',
                             chdir='debuginfo')

  test.built_file_must_exist('bundle_shared_library.framework.dSYM',
                             chdir='debuginfo')
  test.built_file_must_exist('bundle_loadable_module.bundle.dSYM',
                             chdir='debuginfo')
  test.built_file_must_exist('My App.app.dSYM',
                             chdir='debuginfo')

  test.pass_test()
