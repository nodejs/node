#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies *_wrapper in make_global_settings.
"""

import os
import sys
import TestGyp

test_format = ['ninja']
if sys.platform in ('linux2', 'darwin'):
  test_format += ['make']

test = TestGyp.TestGyp(formats=test_format)

old_env = dict(os.environ)
os.environ['GYP_CROSSCOMPILE'] = '1'
test.run_gyp('wrapper.gyp')
os.environ.clear()
os.environ.update(old_env)

if test.format == 'make':
  cc_expected = """ifneq (,$(filter $(origin CC), undefined default))
  CC = $(abspath distcc) $(abspath clang)
endif
"""
  link_expected = 'LINK ?= $(abspath distlink) $(abspath clang++)'
  test.must_contain('Makefile', cc_expected)
  test.must_contain('Makefile', link_expected)
if test.format == 'ninja':
  cc_expected = ('cc = ' + os.path.join('..', '..', 'distcc') + ' ' +
                 os.path.join('..', '..', 'clang'))
  cc_host_expected = ('cc_host = ' + os.path.join('..', '..', 'ccache') + ' ' +
                      os.path.join('..', '..', 'clang'))
  ld_expected = 'ld = ../../distlink $cc'
  if sys.platform == 'win32':
     ld_expected = 'link.exe'
  test.must_contain('out/Default/build.ninja', cc_expected)
  test.must_contain('out/Default/build.ninja', cc_host_expected)
  test.must_contain('out/Default/build.ninja', ld_expected)

test.pass_test()
