#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that action input/output filenames with spaces are rejected.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['android'])

stderr = ('gyp: Action input filename "name with spaces" in target do_actions '
          'contains a space\n')
test.run_gyp('space_filenames.gyp', status=1, stderr=stderr)

test.pass_test()
