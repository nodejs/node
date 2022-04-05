# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.
USE_PYTHON3 = True


def _RunTests(input_api, output_api):
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTestsInDirectory(
          input_api, output_api, '.', files_to_check=[r'.+_test\.py$']))


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  checks = [
    _RunTests,
  ]

  return sum([check(input_api, output_api) for check in checks], [])

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)
