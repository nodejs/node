# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.
USE_PYTHON3 = True

TEST_DIRECTORIES = [
    'unittests',
    'builtins-pgo',
]


def CheckChangeOnCommit(input_api, output_api):
  tests = []
  for directory in TEST_DIRECTORIES:
    tests += input_api.canned_checks.GetUnitTestsInDirectory(
        input_api,
        output_api,
        directory,
        files_to_check=[r'.+_test\.py$'],
        run_on_python2=False)

  return input_api.RunTests(tests)
