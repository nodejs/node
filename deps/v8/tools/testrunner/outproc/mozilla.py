# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base


def _is_failure_output(self, output):
  return (
    output.exit_code != 0 or
    'FAILED!' in output.stdout
  )


class OutProc(base.OutProc):
  """Optimized for positive tests."""
OutProc._is_failure_output = _is_failure_output


class PassOutProc(base.PassOutProc):
  """Optimized for positive tests expected to PASS."""
PassOutProc._is_failure_output = _is_failure_output


class NegOutProc(base.Negative, OutProc):
  pass

class NegPassOutProc(base.Negative, PassOutProc):
  pass


MOZILLA_PASS_DEFAULT = PassOutProc()
MOZILLA_PASS_NEGATIVE = NegPassOutProc()
