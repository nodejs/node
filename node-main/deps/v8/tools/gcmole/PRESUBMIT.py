# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def _RunTests(input_api, output_api):
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTestsInDirectory(
          input_api,
          output_api,
          '.',
          files_to_check=[r'.+_test\.py$'],
          run_on_python2=False))


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
