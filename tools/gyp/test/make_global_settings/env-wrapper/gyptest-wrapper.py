#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies *_wrapper in environment.
"""

from __future__ import print_function

import os
import sys
import TestGyp

print("This test is currently disabled: https://crbug.com/483696.")
sys.exit(0)

test_format = ['ninja']

os.environ['CC_wrapper'] = 'distcc'
os.environ['LINK_wrapper'] = 'distlink'
os.environ['CC.host_wrapper'] = 'ccache'

test = TestGyp.TestGyp(formats=test_format)

old_env = dict(os.environ)
os.environ['GYP_CROSSCOMPILE'] = '1'
test.run_gyp('wrapper.gyp')
os.environ.clear()
os.environ.update(old_env)

if test.format == 'ninja':
  cc_expected = ('cc = ' + os.path.join('..', '..', 'distcc') + ' ' +
                 os.path.join('..', '..', 'clang'))
  cc_host_expected = ('cc_host = ' + os.path.join('..', '..', 'ccache') + ' ' +
                      os.path.join('..', '..', 'clang'))
  ld_expected = 'ld = ../../distlink $cc'
  if sys.platform != 'win32':
    ldxx_expected = 'ldxx = ../../distlink $cxx'

  if sys.platform == 'win32':
     ld_expected = 'link.exe'
  test.must_contain('out/Default/build.ninja', cc_expected)
  test.must_contain('out/Default/build.ninja', cc_host_expected)
  test.must_contain('out/Default/build.ninja', ld_expected)
  if sys.platform != 'win32':
    test.must_contain('out/Default/build.ninja', ldxx_expected)

test.pass_test()
