#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies props files are added by using a
props file to set the name of the built executable.
"""

import TestGyp

test = TestGyp.TestGyp(workdir='workarea_all', formats=['msvs'])

test.run_gyp('hello.gyp')

test.build('hello.gyp')

test.built_file_must_exist('Greet.exe')

test.pass_test()
