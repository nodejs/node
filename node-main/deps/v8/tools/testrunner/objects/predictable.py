# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from ..outproc import base as outproc_base
from ..testproc import base as testproc_base


# Only check the exit code of the predictable_wrapper in
# verify-predictable mode. Negative tests are not supported as they
# usually also don't print allocation hashes. There are two versions of
# negative tests: one specified by the test, the other specified through
# the status file (e.g. known bugs).


def get_outproc(test):
  return OutProc(test.output_proc)


class OutProc(outproc_base.BaseOutProc):
  """Output processor wrapper for predictable mode. It has custom process and
  has_unexpected_output implementation, but for all other methods it simply
  calls wrapped output processor.
  """
  def __init__(self, _outproc):
    super(OutProc, self).__init__()
    self._outproc = _outproc

  def has_unexpected_output(self, output):
    return output.exit_code != 0

  def get_outcome(self, output):
    return self._outproc.get_outcome(output)

  @property
  def negative(self):
    return self._outproc.negative

  @property
  def expected_outcomes(self):
    return self._outproc.expected_outcomes


class PredictableFilterProc(testproc_base.TestProcFilter):
  def _filter(self, test):
    return test.skip_predictable()
