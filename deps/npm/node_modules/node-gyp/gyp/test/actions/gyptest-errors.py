#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies behavior for different action configuration errors:
exit status of 1, and the expected error message must be in stderr.
"""

import TestGyp

test = TestGyp.TestGyp(workdir='workarea_errors')


test.run_gyp('action_missing_name.gyp', chdir='src', status=1, stderr=None)
expect = [
  "Anonymous action in target broken_actions2.  An action must have an 'action_name' field.",
]
test.must_contain_all_lines(test.stderr(), expect)


test.pass_test()
