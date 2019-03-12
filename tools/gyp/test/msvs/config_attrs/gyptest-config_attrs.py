#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that msvs_configuration_attributes and
msbuild_configuration_attributes are applied by using
them to set the OutputDirectory.
"""

from __future__ import print_function

import TestGyp
import os

import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)



test = TestGyp.TestGyp(workdir='workarea_all',formats=['msvs'])

vc_version = 'VC90'

if os.getenv('GYP_MSVS_VERSION'):
  vc_version = ['VC90','VC100'][int(os.getenv('GYP_MSVS_VERSION')) >= 2010]

expected_exe_file = os.path.join(test.workdir, vc_version, 'hello.exe')

test.run_gyp('hello.gyp')

test.build('hello.gyp')

test.built_file_must_exist(expected_exe_file)

test.pass_test()
