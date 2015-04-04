#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simple actions when using an explicit build target of 'all'.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('all.gyp',
             '-G', 'xcode_ninja_target_pattern=^all_targets$',
             chdir='src')

test.relocate('src', 'relocate/src')

# Build all.
test.build('all.gyp', chdir='relocate/src')

if test.format=='xcode':
  chdir = 'relocate/src/dir1'
else:
  chdir = 'relocate/src'

# Output is as expected.
file_content = 'Hello from emit.py\n'
test.built_file_must_match('out2.txt', file_content, chdir=chdir)

test.built_file_must_not_exist('out.txt', chdir='relocate/src')
test.built_file_must_not_exist('foolib1',
                               type=test.SHARED_LIB,
                               chdir=chdir)

# xcode-ninja doesn't generate separate workspaces for sub-gyps by design
if test.format == 'xcode-ninja':
  test.pass_test()

# TODO(mmoss) Make consistent with msvs, with 'dir1' before 'out/Default'?
if test.format in ('make', 'ninja', 'android', 'cmake'):
  chdir='relocate/src'
else:
  chdir='relocate/src/dir1'

# Build the action explicitly.
test.build('actions.gyp', 'action1_target', chdir=chdir)

# Check that things got run.
file_content = 'Hello from emit.py\n'
test.built_file_must_exist('out.txt', chdir=chdir)

# Build the shared library explicitly.
test.build('actions.gyp', 'foolib1', chdir=chdir)

test.built_file_must_exist('foolib1',
                           type=test.SHARED_LIB,
                           chdir=chdir,
                           subdir='dir1')

test.pass_test()
