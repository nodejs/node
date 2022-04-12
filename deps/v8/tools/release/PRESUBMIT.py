# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.
USE_PYTHON3 = True


def _CommonChecks(input_api, output_api):
  tests = input_api.canned_checks.GetUnitTestsInDirectory(
      input_api, output_api, '.', files_to_check=['test_scripts.py$'],
      run_on_python2=False)
  return input_api.RunTests(tests)

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
