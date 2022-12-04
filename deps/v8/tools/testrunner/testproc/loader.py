# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base

class LoadProc(base.TestProc):
  """First processor in the chain that passes all tests to the next processor.
  """

  def __init__(self, tests, initial_batch_size=float('inf')):
    super(LoadProc, self).__init__()

    self.tests = tests
    self.initial_batch_size = initial_batch_size


  def load_initial_tests(self):
    """Send an initial batch of tests down to executor"""
    if not self.initial_batch_size:
      return
    to_load = self.initial_batch_size
    for t in self.tests:
      if self._send_test(t):
        to_load -= 1
      if not to_load:
        break

  def next_test(self, test):
    assert False, \
        'Nothing can be connected to the LoadProc' # pragma: no cover

  def result_for(self, test, result):
    for t in self.tests:
      if self._send_test(t):
        break
