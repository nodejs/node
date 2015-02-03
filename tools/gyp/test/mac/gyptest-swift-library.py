#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a swift framework builds correctly.
"""

import TestGyp
import TestMac

import collections
import sys
import subprocess

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['xcode'])

  # Ensures that the given symbol is present in the given file, by running nm.
  def CheckHasSymbolName(path, symbol):
    output = subprocess.check_output(['nm', '-j', path])
    idx = output.find(symbol)
    if idx == -1:
      print 'Swift: Could not find symobl: %s' % symbol
      test.fail_test()

  test_cases = []

  # Run this for iOS on XCode 6.0 or greater
  if TestMac.Xcode.Version() >= '0600':
    test_cases.append(('Default', 'iphoneos'))
    test_cases.append(('Default', 'iphonesimulator'))

  # Run it for Mac on XCode 6.1 or greater
  if TestMac.Xcode.Version() >= '0610':
    test_cases.append(('Default', None))

  # Generate the project.
  test.run_gyp('test.gyp', chdir='swift-library')

  # Build and verify for each configuration.
  for configuration, sdk in test_cases:
    kwds = collections.defaultdict(list)
    if test.format == 'xcode':
      if sdk is not None:
        kwds['arguments'].extend(['-sdk', sdk])

    test.set_configuration(configuration)
    test.build('test.gyp', 'SwiftFramework', chdir='swift-library', **kwds)

    filename = 'SwiftFramework.framework/SwiftFramework'
    result_file = test.built_file_path(filename, chdir='swift-library')

    test.must_exist(result_file)

    # Check to make sure that our swift class (GypSwiftTest) is present in the
    # built binary
    CheckHasSymbolName(result_file, "C14SwiftFramework12GypSwiftTest")

  test.pass_test()
