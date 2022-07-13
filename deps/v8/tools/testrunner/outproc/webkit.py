# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base


class OutProc(base.ExpectedOutProc):
  def _is_failure_output(self, output):
    if output.exit_code != 0:
      return True
    return super(OutProc, self)._is_failure_output(output)

  def _ignore_expected_line(self, line):
    return (
        line.startswith('#') or
        super(OutProc, self)._ignore_expected_line(line)
    )
