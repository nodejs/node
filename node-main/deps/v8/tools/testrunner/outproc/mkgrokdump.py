# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib

from . import base

class OutProc(base.OutProc):

  def __init__(self, expected_outcomes):
    super(OutProc, self).__init__(expected_outcomes)

  def _is_failure_output(self, output):
    MIN_EXPECTED_LINE_COUNT = 300  # Currently the actual count is 652.
    if len(output.stdout.splitlines()) < MIN_EXPECTED_LINE_COUNT:
      output.stdout = ("Plausibility check failed. Output too short.\n\n" +
                       output.stdout)
      return True

    PLAUSIBILITY_CHECK_EXPECTED_STRINGS = [
        "INSTANCE_TYPES = {",
        "HEAP_NUMBER_TYPE",
        "KNOWN_MAPS = {",
        "HeapNumberMap",
        "KNOWN_OBJECTS = {",
        "TrueValue",
        "HEAP_FIRST_PAGES = {",
        "FRAME_MARKERS = (",
        "INTERPRETED",
    ]
    for expected_string in PLAUSIBILITY_CHECK_EXPECTED_STRINGS:
      if not expected_string in output.stdout:
        output.stdout = (
            "Plausibility check failed. Expected string "
            f"'{expected_string}' not found in output.\n") + output.stdout
        return True

    return False
