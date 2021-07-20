# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import deque

from . import base


class SequenceProc(base.TestProc):
  """Processor ensuring heavy tests are sent sequentially into the execution
  pipeline.

  The class keeps track of the number of tests in the pipeline marked heavy
  and permits only a configurable amount. An excess amount is queued and sent
  as soon as other heavy tests return.
  """
  def __init__(self, max_heavy):
    """Initialize the processor.

    Args:
      max_heavy: The maximum number of heavy tests that will be sent further
                 down the pipeline simultaneously.
    """
    super(SequenceProc, self).__init__()
    assert max_heavy > 0
    self.max_heavy = max_heavy
    self.n_heavy = 0
    self.buffer = deque()

  def next_test(self, test):
    if test.is_heavy:
      if self.n_heavy < self.max_heavy:
        # Enough space to send more heavy tests. Check if the test is not
        # filtered otherwise.
        used = self._send_test(test)
        if used:
          self.n_heavy += 1
        return used
      else:
        # Too many tests in the pipeline. Buffer the test and indicate that
        # this test didn't end up in the execution queue (i.e. test loader
        # will try to send more tests).
        self.buffer.append(test)
        return False
    else:
      return self._send_test(test)

  def result_for(self, test, result):
    if test.is_heavy:
      # A heavy test finished computing. Try to send one from the buffer.
      self.n_heavy -= 1
      while self.buffer:
        next_test = self.buffer.popleft()
        if self._send_test(next_test):
          self.n_heavy += 1
          break

    self._send_result(test, result)
