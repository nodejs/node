# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def CheckChangeOnCommit(input_api, output_api):
  # TODO(machenbach): Run all unittests.
  tests = input_api.canned_checks.GetUnitTestsInDirectory(
      input_api, output_api, '.', whitelist=['run_tests_test.py$'])
  return input_api.RunTests(tests)
