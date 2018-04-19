# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from . import base


class TimeoutProc(base.TestProcObserver):
  def __init__(self, duration_sec):
    super(TimeoutProc, self).__init__()
    self._duration_sec = duration_sec
    self._start = time.time()

  def _on_next_test(self, test):
    self._on_event()

  def _on_result_for(self, test, result):
    self._on_event()

  def _on_heartbeat(self):
    self._on_event()

  def _on_event(self):
    if not self.is_stopped:
      if time.time() - self._start > self._duration_sec:
        self.stop()
