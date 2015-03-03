#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that an Info.plist with CFBundleSignature works.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='missing-cfbundlesignature')
  test.build('test.gyp', test.ALL, chdir='missing-cfbundlesignature')

  test.built_file_must_match('mytarget.app/Contents/PkgInfo', 'APPL????',
                             chdir='missing-cfbundlesignature')

  test.built_file_must_match('myothertarget.app/Contents/PkgInfo', 'APPL????',
                             chdir='missing-cfbundlesignature')

  test.built_file_must_match('thirdtarget.app/Contents/PkgInfo', 'APPL????',
                             chdir='missing-cfbundlesignature')
  test.pass_test()
