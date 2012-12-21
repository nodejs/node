#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test that two targets with the same name generates an error.
"""

import TestGyp
import TestCmd

# TODO(sbc): Remove the need for match_re below, and make scons
# error messages consistent with other generators by removing
# input.py:generator_wants_absolute_build_file_paths.

test = TestGyp.TestGyp()

stderr = ('gyp: Duplicate target definitions for '
          '.*duplicate_targets.gyp:foo#target\n')
test.run_gyp('duplicate_targets.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re)

stderr = ('gyp: Unable to find targets in build file .*missing_targets.gyp '
          'while trying to load missing_targets.gyp\n')
test.run_gyp('missing_targets.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re)

stderr = ('gyp: rule bar exists in duplicate, target '
          '.*duplicate_rule.gyp:foo#target\n')
test.run_gyp('duplicate_rule.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re)

stderr = ("gyp: Key 'targets' repeated at level 1 with key path '' while "
          "reading .*duplicate_node.gyp while trying to load "
          "duplicate_node.gyp\n")
test.run_gyp('duplicate_node.gyp', '--check', status=1, stderr=stderr,
             match=TestCmd.match_re)

stderr = 'gyp: Duplicate basenames in sources section, see list above\n'
test.run_gyp('duplicate_basenames.gyp', status=1, stderr=stderr)

stderr = ("gyp: Dependency '.*missing_dep.gyp:missing.gyp#target' not found "
          "while trying to load target .*missing_dep.gyp:foo#target\n")
test.run_gyp('missing_dep.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re)

test.pass_test()
