#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ios app extensions are built correctly.
"""

import TestGyp
import TestMac

import sys
if sys.platform == 'darwin' and TestMac.Xcode.Version()>="0600":
  test = TestGyp.TestGyp(formats=['ninja', 'xcode'])

  test.run_gyp('extension.gyp', chdir='extension')

  test.build('extension.gyp', 'ExtensionContainer', chdir='extension')

  # Test that the extension is .appex
  test.built_file_must_exist(
      'ExtensionContainer.app/PlugIns/ActionExtension.appex',
      chdir='extension')

  test.pass_test()

