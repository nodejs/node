# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base

from testrunner.local import statusfile
from testrunner.outproc import base as outproc

class ForgiveTimeoutProc(base.TestProcProducer):
  """Test processor passing tests and results through and forgiving timeouts."""
  def __init__(self):
    super(ForgiveTimeoutProc, self).__init__('no-timeout')

  def _next_test(self, test):
    subtest = self._create_subtest(test, 'no_timeout')
    if subtest.expected_outcomes == outproc.OUTCOMES_PASS:
      subtest.expected_outcomes = outproc.OUTCOMES_PASS_OR_TIMEOUT
    elif subtest.expected_outcomes == outproc.OUTCOMES_FAIL:
      subtest.expected_outcomes = outproc.OUTCOMES_FAIL_OR_TIMEOUT
    elif statusfile.TIMEOUT not in subtest.expected_outcomes:
      subtest.expected_outcomes = (
          subtest.expected_outcomes + [statusfile.TIMEOUT])

    return self._send_test(subtest)

  def _result_for(self, test, subtest, result):
    self._send_result(test, result)
