# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def _CommonChecks(input_api, output_api):
  tests = []
  for directory in ('scripts', 'skills/clusterfuzz/scripts'):
    tests.extend(
        input_api.canned_checks.GetUnitTestsInDirectory(
            input_api,
            output_api,
            directory,
            files_to_check=[r'.+_test\.py$'],
            run_on_python2=False))
  return input_api.RunTests(tests)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)
