#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test variable expansion of '<!()' syntax commands.
"""

import os

import TestGyp

test = TestGyp.TestGyp(format='gypd')

expect = test.read('commands.gyp.stdout').replace('\r', '')

test.run_gyp('commands.gyp',
             '--debug', 'variables', '--debug', 'general',
             stdout=expect)

# Verify the commands.gypd against the checked-in expected contents.
#
# Normally, we should canonicalize line endings in the expected
# contents file setting the Subversion svn:eol-style to native,
# but that would still fail if multiple systems are sharing a single
# workspace on a network-mounted file system.  Consequently, we
# massage the Windows line endings ('\r\n') in the output to the
# checked-in UNIX endings ('\n').

contents = test.read('commands.gypd').replace('\r', '')
expect = test.read('commands.gypd.golden').replace('\r', '')
if not test.match(contents, expect):
  print "Unexpected contents of `commands.gypd'"
  test.diff(expect, contents, 'commands.gypd ')
  test.fail_test()

test.pass_test()
