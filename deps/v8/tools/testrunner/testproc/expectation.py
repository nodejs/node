# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base

class ExpectationProc(base.TestProcProducer):
  """Test processor passing tests and results through and forgiving timeouts."""
  def __init__(self):
    super(ExpectationProc, self).__init__('no-timeout')

  def _next_test(self, test):
    subtest = test.create_subtest(self, 'no_timeout')
    subtest.allow_timeouts()
    subtest.allow_pass()
    return self._send_test(subtest)

  def _result_for(self, test, subtest, result):
    self._send_result(test, result)
