#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that msvs_configuration_attributes and
msbuild_configuration_attributes are applied by using
them to set the OutputDirectory.
"""

import TestGyp
import os

test = TestGyp.TestGyp(workdir='workarea_all',formats=['msvs'])

vc_version = 'VC90'

if os.getenv('GYP_MSVS_VERSION'):
  vc_version = ['VC90','VC100'][int(os.getenv('GYP_MSVS_VERSION')) >= 2010]

expected_exe_file = os.path.join(test.workdir, vc_version, 'hello.exe')

test.run_gyp('hello.gyp')

test.build('hello.gyp')

test.must_exist(expected_exe_file)

test.pass_test()
