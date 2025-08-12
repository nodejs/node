# Copyright 2025 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base

import platform
import subprocess


class FileCheckOutProc(base.OutProc):
  """Processor for FileCheck-based expectations."""

  def __init__(self, expected_outcomes, js_file):
    super(FileCheckOutProc, self).__init__(expected_outcomes)
    self._js_file = js_file

  def _is_failure_output(self, output):
    cmd = ["vpython3", "-m", "filecheck", str(self._js_file)]
    # Note that we encode the input again because on Windows providing
    # > input=output.stdout, encoding="ascii", text=True
    # creates some issues with the filecheck CHECK-NEXT directive (maybe
    # something goes wrong in terms of new lines)...
    res = subprocess.run(
        cmd,
        input=output.stdout.encode("ascii"),
        capture_output=True,
        shell=platform.system() == 'Windows')
    if res.returncode != 0 and "FileCheck failed" not in output.stderr:
      # FileCheck failed, add error to stderr (even though technically not
      # correct) to report it back in the various UIs of testrunners.
      output.stderr += "\nFileCheck failed:\n" + res.stderr.decode()
    return res.returncode != 0
