#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that setting SDKROOT works.
"""

import TestGyp

import os
import subprocess
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  def GetSDKPath(sdk):
    """Return SDKROOT if the SDK version |sdk| is installed or empty string."""
    DEVNULL = open(os.devnull, 'wb')
    try:
      proc = subprocess.Popen(
          ['xcodebuild', '-version', '-sdk', 'macosx' + sdk, 'Path'],
          stdout=subprocess.PIPE, stderr=DEVNULL)
      return proc.communicate()[0].rstrip('\n')
    finally:
      DEVNULL.close()

  def SelectSDK():
    """Select the oldest SDK installed (greater than 10.6)."""
    for sdk in ['10.6', '10.7', '10.8', '10.9']:
      path = GetSDKPath(sdk)
      if path:
        return True, sdk, path
    return False, '', ''

  # Make sure this works on the bots, which only have the 10.6 sdk, and on
  # dev machines which usually don't have the 10.6 sdk.
  sdk_found, sdk, sdk_path = SelectSDK()
  if not sdk_found:
    test.fail_test()

  test.write('sdkroot/test.gyp', test.read('sdkroot/test.gyp') % sdk)

  test.run_gyp('test.gyp', '-D', 'sdk_path=%s' % sdk_path,
               chdir='sdkroot')
  test.build('test.gyp', test.ALL, chdir='sdkroot')
  test.pass_test()
