#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test that two targets with the same name generates an error.
"""

import TestGyp
from SConsLib import TestCmd

# TODO(sbc): Remove the use of match_re below, done because scons
# error messages were not consistent with other generators.
# Also remove input.py:generator_wants_absolute_build_file_paths.

test = TestGyp.TestGyp()

stderr = 'gyp: Duplicate target definitions for .*duplicate_targets.gyp:foo#target\n'
test.run_gyp('duplicate_targets.gyp', status=1, stderr=stderr, match=TestCmd.match_re)

stderr = '.*: Unable to find targets in build file .*missing_targets.gyp.*'
test.run_gyp('missing_targets.gyp', status=1, stderr=stderr, match=TestCmd.match_re_dotall)

stderr = 'gyp: rule bar exists in duplicate, target .*duplicate_rule.gyp:foo#target\n'
test.run_gyp('duplicate_rule.gyp', status=1, stderr=stderr, match=TestCmd.match_re)

stderr = "gyp: Key 'targets' repeated at level 1 with key path '' while reading .*duplicate_node.gyp.*"
test.run_gyp('duplicate_node.gyp', '--check', status=1, stderr=stderr, match=TestCmd.match_re_dotall)

stderr = ".*target2.*target0.*target1.*target2.*"
test.run_gyp('dependency_cycle.gyp', status=1, stderr=stderr, match=TestCmd.match_re_dotall)

stderr = ".*file_cycle1.*file_cycle0.*file_cycle1.*"
test.run_gyp('file_cycle0.gyp', status=1, stderr=stderr, match=TestCmd.match_re_dotall)

stderr = 'gyp: Duplicate basenames in sources section, see list above\n'
test.run_gyp('duplicate_basenames.gyp', status=1, stderr=stderr)

stderr = "gyp: Dependency '.*missing_dep.gyp:missing.gyp#target' not found while trying to load target .*missing_dep.gyp:foo#target\n"
test.run_gyp('missing_dep.gyp', status=1, stderr=stderr, match=TestCmd.match_re)

# Make sure invalid <!() command invocations say what command it was and
# mention the gyp file name. Use a "random" command name to trigger an ENOENT.
stderr = ".*invalid-command-name-egtyevNif3.*netDurj9.*missing_command.gyp.*"
test.run_gyp('missing_command.gyp', status=1, stderr=stderr, match=TestCmd.match_re_dotall)

# Make sure <!() commands that error out result in a message that mentions
# the command and gyp file name
stderr = ".*python.*-c.*import sys.*sys.exit.*3.*error_command.gyp.*"
test.run_gyp('error_command.gyp', status=1, stderr=stderr, match=TestCmd.match_re_dotall)

test.pass_test()
