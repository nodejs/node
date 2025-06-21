# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base


class OutProc(base.ExpectedOutProc):
  def _is_failure_output(self, output):
    if output.exit_code != 0:
      return True
    if output.exit_code == 0 and not output.stdout.strip():
      # Special case for --exit-on-contradictory-flags. It lets tests exit
      # fast with exit code 0. Normally passing webkit tests all have output.
      return False
    return super(OutProc, self)._is_failure_output(output)

  def _ignore_expected_line(self, line):
    return (
        line.startswith('#') or
        super(OutProc, self)._ignore_expected_line(line)
    )
