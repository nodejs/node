# Copyright 2025 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import platform
from . import base

# -1 gets converted to 0xff on POSIX and 0xffffffff on Windows.
MINUS_ONE_EXIT_CODE = 0xffffffff if platform.system() == 'Windows' else 0xff


class OutProc(base.OutProc):
  """
  Fuzzers are allowed to return -1 or 0 (see
  https://llvm.org/docs/LibFuzzer.html#rejecting-unwanted-inputs).
  """

  def _is_failure_output(self, output):
    return output.exit_code != 0 and output.exit_code != MINUS_ONE_EXIT_CODE
