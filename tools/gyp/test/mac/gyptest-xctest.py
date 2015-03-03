#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that xctest targets are correctly configured.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['xcode'])

  # Ignore this test if Xcode 5 is not installed
  import subprocess
  job = subprocess.Popen(['xcodebuild', '-version'],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
  out, err = job.communicate()
  if job.returncode != 0:
    raise Exception('Error %d running xcodebuild' % job.returncode)
  xcode_version, build_number = out.splitlines()
  # Convert the version string from 'Xcode 5.0' to ['5','0'].
  xcode_version = xcode_version.split()[-1].split('.')
  if xcode_version < ['5']:
    test.pass_test()

  CHDIR = 'xctest'
  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', chdir=CHDIR, arguments=['-scheme', 'classes', 'test'])

  test.built_file_must_match('tests.xctest/Contents/Resources/resource.txt',
                             'foo\n', chdir=CHDIR)
  test.pass_test()
