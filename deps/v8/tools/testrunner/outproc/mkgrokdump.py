# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib

from . import base


class OutProc(base.OutProc):
  def __init__(self, expected_outcomes, expected_path):
    super(OutProc, self).__init__(expected_outcomes)
    self._expected_path = expected_path

  def _is_failure_output(self, output):
    with open(self._expected_path) as f:
      expected = f.read()
    expected_lines = expected.splitlines()
    actual_lines = output.stdout.splitlines()
    diff = difflib.unified_diff(expected_lines, actual_lines, lineterm="",
                                fromfile="expected_path")
    diffstring = '\n'.join(diff)
    if diffstring is not "":
      if "generated from a non-shipping build" in output.stdout:
        return False
      if not "generated from a shipping build" in output.stdout:
        output.stdout = "Unexpected output:\n\n" + output.stdout
        return True
      output.stdout = diffstring
      return True
    return False
