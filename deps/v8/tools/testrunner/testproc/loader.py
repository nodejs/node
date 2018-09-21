# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base


class LoadProc(base.TestProc):
  """First processor in the chain that passes all tests to the next processor.
  """

  def load_tests(self, tests):
    loaded = set()
    for test in tests:
      if test.procid in loaded:
        print 'Warning: %s already obtained' % test.procid
        continue

      loaded.add(test.procid)
      self._send_test(test)

  def next_test(self, test):
    assert False, 'Nothing can be connected to the LoadProc'

  def result_for(self, test, result):
    # Ignore all results.
    pass
