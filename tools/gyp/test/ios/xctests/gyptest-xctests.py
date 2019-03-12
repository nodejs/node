#!/usr/bin/env python

# Copyright 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that iOS XCTests can be built correctly.
"""

import TestGyp

import os
import subprocess
import sys

def HasCerts():
  # Because the bots do not have certs, don't check them if there are no
  # certs available.
  proc = subprocess.Popen(['security','find-identity','-p', 'codesigning',
                           '-v'], stdout=subprocess.PIPE)
  return "0 valid identities found" not in proc.communicate()[0].strip()

if sys.platform == "darwin":
  # This test appears to be flaky and hangs some of the time.
  sys.exit(2)  # bug=531

  test = TestGyp.TestGyp(formats=['xcode', 'ninja'])
  test.run_gyp('xctests.gyp')
  test_configs = ['Default']
  # TODO(crbug.com/557418): Enable this once xcodebuild works for iOS devices.
  #if HasCerts() and test.format == 'xcode':
  #  test_configs.append('Default-iphoneos')
  for config in test_configs:
    test.set_configuration(config)
    test.build('xctests.gyp', test.ALL)
    test.built_file_must_exist('app_under_test.app/app_under_test')
    test.built_file_must_exist('app_tests.xctest/app_tests')
    if 'ninja' in test.format:
      test.built_file_must_exist('obj/AppTests/app_tests.AppTests.i386.o')
      test.built_file_must_exist('obj/AppTests/app_tests.AppTests.x86_64.o')
    elif test.format == 'xcode':
      xcode_object_path = os.path.join('..', 'xctests.build',
                                       'Default-iphonesimulator',
                                       'app_tests.build', 'Objects-normal',
                                       '%s', 'AppTests.o')
      test.built_file_must_exist(xcode_object_path % 'i386')
      test.built_file_must_exist(xcode_object_path % 'x86_64')
  test.pass_test()
