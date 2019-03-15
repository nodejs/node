# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base


class LoadProc(base.TestProc):
  """First processor in the chain that passes all tests to the next processor.
  """

  def __init__(self, tests):
    super(LoadProc, self).__init__()

    self.tests = tests

  def load_initial_tests(self, initial_batch_size):
    """
    Args:
      exec_proc: execution processor that the tests are being loaded into
      initial_batch_size: initial number of tests to load
    """
    loaded_tests = 0
    while loaded_tests < initial_batch_size:
      try:
        t = next(self.tests)
      except StopIteration:
        return

      if self._send_test(t):
        loaded_tests += 1

  def next_test(self, test):
    assert False, 'Nothing can be connected to the LoadProc'

  def result_for(self, test, result):
    try:
      while not self._send_test(next(self.tests)):
        pass
    except StopIteration:
      # No more tests to load.
      pass
