#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ios watch extensions and apps are built correctly.
"""

import TestGyp
import TestMac

import sys
if sys.platform == 'darwin' and TestMac.Xcode.Version() >= "0620":
  test = TestGyp.TestGyp(formats=['ninja', 'xcode'])

  test.run_gyp('watch.gyp', chdir='watch')

  test.build(
      'watch.gyp',
      'WatchContainer',
      chdir='watch')

  # Test that the extension exists
  test.built_file_must_exist(
      'WatchContainer.app/PlugIns/WatchKitExtension.appex',
      chdir='watch')

  # Test that the watch app exists
  test.built_file_must_exist(
      'WatchContainer.app/PlugIns/WatchKitExtension.appex/WatchApp.app',
      chdir='watch')

  test.pass_test()

