#!/usr/bin/env python

# Copyright 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that configurations do not duplicate other settings.
"""

import TestGyp

test = TestGyp.TestGyp(format='gypd')

test.run_gyp('duplicates.gyp')

# Verify the duplicates.gypd against the checked-in expected contents.
#
# Normally, we should canonicalize line endings in the expected
# contents file setting the Subversion svn:eol-style to native,
# but that would still fail if multiple systems are sharing a single
# workspace on a network-mounted file system.  Consequently, we
# massage the Windows line endings ('\r\n') in the output to the
# checked-in UNIX endings ('\n').

contents = test.read('duplicates.gypd').replace(
    '\r', '').replace('\\\\', '/')
expect = test.read('duplicates.gypd.golden').replace('\r', '')
if not test.match(contents, expect):
  print "Unexpected contents of `duplicates.gypd'"
  test.diff(expect, contents, 'duplicates.gypd ')
  test.fail_test()

test.pass_test()
