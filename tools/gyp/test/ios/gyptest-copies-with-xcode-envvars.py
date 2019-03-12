#!/usr/bin/env python

# Copyright (c) 2016 Mark Callow. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that files are copied to the correct destinations when those
destinations are specified using environment variables available in
Xcode's PBXCopyFilesBuildPhase.
"""

import TestGyp

import os
import stat
import sys


test = TestGyp.TestGyp(formats=['ninja', 'xcode'])

if sys.platform == 'darwin':
  test.run_gyp('copies-with-xcode-envvars.gyp',
                chdir='copies-with-xcode-envvars')

  test.build('copies-with-xcode-envvars.gyp', chdir='copies-with-xcode-envvars')

  wrapper_name = 'copies-with-xcode-envvars.app/'
  contents_path = wrapper_name
  out_path = test.built_file_path('file0', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file0 contents\n')
  out_path = test.built_file_path(wrapper_name + 'file1', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file1 contents\n')
  out_path = test.built_file_path(contents_path + 'file2', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file2 contents\n')
  out_path = test.built_file_path(contents_path + 'file3', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file3 contents\n')
  out_path = test.built_file_path(contents_path + 'testimages/file4', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file4 contents\n')
  out_path = test.built_file_path(contents_path + 'Java/file5', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file5 contents\n')
  out_path = test.built_file_path(contents_path + 'Frameworks/file6', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file6 contents\n')
  out_path = test.built_file_path(contents_path + 'Frameworks/file7', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file7 contents\n')
  out_path = test.built_file_path(contents_path + 'SharedFrameworks/file8', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file8 contents\n')
  out_path = test.built_file_path(contents_path + 'SharedSupport/file9', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file9 contents\n')
  out_path = test.built_file_path(contents_path + 'PlugIns/file10', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file10 contents\n')
  out_path = test.built_file_path(contents_path + 'XPCServices/file11', chdir='copies-with-xcode-envvars')
  test.must_contain(out_path, 'file11 contents\n')
  test.pass_test()
