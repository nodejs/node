#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that device and simulator bundles are built correctly.
"""

import TestGyp
import TestMac

import collections
import sys


if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'xcode'])

  test_cases = [
    ('Default', 'TestArch32Bits', ['i386']),
    ('Default-iphoneos', 'TestArch32Bits', ['armv7']),
  ]

  if TestMac.Xcode.Version() < '0510':
    test_cases.extend([
        ('Default', 'TestNoArchs', ['i386']),
        ('Default-iphoneos', 'TestNoArchs', ['armv7'])])

  if TestMac.Xcode.Version() >= '0500':
    test_cases.extend([
        ('Default', 'TestArch64Bits', ['x86_64']),
        ('Default', 'TestMultiArchs', ['i386', 'x86_64']),
        ('Default-iphoneos', 'TestArch64Bits', ['arm64']),
        ('Default-iphoneos', 'TestMultiArchs', ['armv7', 'arm64'])])

  test.run_gyp('test-archs.gyp', chdir='app-bundle')
  for configuration, target, archs in test_cases:
    is_device_build = configuration.endswith('-iphoneos')

    kwds = collections.defaultdict(list)
    if test.format == 'xcode':
      if is_device_build:
        configuration, sdk = configuration.split('-')
        kwds['arguments'].extend(['-sdk', sdk])
      if TestMac.Xcode.Version() < '0500':
        kwds['arguments'].extend(['-arch', archs[0]])

    test.set_configuration(configuration)
    filename = '%s.bundle/%s' % (target, target)
    test.build('test-archs.gyp', target, chdir='app-bundle', **kwds)
    result_file = test.built_file_path(filename, chdir='app-bundle')

    test.must_exist(result_file)
    TestMac.CheckFileType(test, result_file, archs)

  test.pass_test()
