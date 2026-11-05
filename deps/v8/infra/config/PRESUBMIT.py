# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.
USE_PYTHON3 = True

LUCICFG_ENTRY_SCRIPTS = ['main.star']


def CheckChangeOnUpload(input_api, output_api):
  tests = []
  for path in LUCICFG_ENTRY_SCRIPTS:
    tests += input_api.canned_checks.CheckLucicfgGenOutput(
        input_api, output_api, path)
  return input_api.RunTests(tests)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChangeOnUpload(input_api, output_api)
