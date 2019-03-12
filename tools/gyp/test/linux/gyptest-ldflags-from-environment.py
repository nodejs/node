#!/usr/bin/env python

# Copyright (c) 2017 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies the use of linker flags in environment variables.

In this test, gyp and build both run in same local environment.
"""

import TestGyp

import re
import subprocess
import sys

FORMATS = ('make', 'ninja')

if sys.platform.startswith('linux'):
  test = TestGyp.TestGyp(formats=FORMATS)

  CHDIR = 'ldflags-from-environment'
  env = {
    'LDFLAGS': '-Wl,--dynamic-linker=/target',
    'LDFLAGS_host': '-Wl,--dynamic-linker=/host',
    'GYP_CROSSCOMPILE': '1'
  }
  with TestGyp.LocalEnv(env):
    test.run_gyp('test.gyp', chdir=CHDIR)
    test.build('test.gyp', chdir=CHDIR)

  ldflags = test.run_readelf('ldflags', CHDIR, '-l')
  if ldflags != ['/target']:
    test.fail_test()

  ldflags_host = test.run_readelf('ldflags_host', CHDIR, '-l')
  if ldflags_host != ['/host']:
    test.fail_test()

  test.pass_test()
