#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test that two targets with the same name generates an error.
"""

import os
import sys

import TestGyp
import TestCmd

# TODO(sbc): Remove the use of match_re below, done because scons
# error messages were not consistent with other generators.
# Also remove input.py:generator_wants_absolute_build_file_paths.

test = TestGyp.TestGyp()

stderr = ('gyp: Duplicate target definitions for '
          '.*duplicate_targets.gyp:foo#target\n')
test.run_gyp('duplicate_targets.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re)

stderr = ('.*: Unable to find targets in build file .*missing_targets.gyp.*')
test.run_gyp('missing_targets.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re_dotall)

stderr = ('gyp: rule bar exists in duplicate, target '
          '.*duplicate_rule.gyp:foo#target\n')
test.run_gyp('duplicate_rule.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re)

stderr = ("gyp: Key 'targets' repeated at level 1 with key path '' while "
          "reading .*duplicate_node.gyp.*")
test.run_gyp('duplicate_node.gyp', '--check', status=1, stderr=stderr,
             match=TestCmd.match_re_dotall)

stderr = (".*target0.*target1.*target2.*target0.*")
test.run_gyp('dependency_cycle.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re_dotall)

stderr = (".*file_cycle0.*file_cycle1.*file_cycle0.*")
test.run_gyp('file_cycle0.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re_dotall)

stderr = ("gyp: Dependency '.*missing_dep.gyp:missing.gyp#target' not found "
          "while trying to load target .*missing_dep.gyp:foo#target\n")
test.run_gyp('missing_dep.gyp', status=1, stderr=stderr,
             match=TestCmd.match_re)

test.pass_test()
