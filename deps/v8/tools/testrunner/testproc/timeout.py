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
    self.__on_event()

  def _on_result_for(self, test, result):
    self.__on_event()

  def _on_heartbeat(self):
    self.__on_event()

  def __on_event(self):
    if not self.is_stopped:
      if time.time() - self._start > self._duration_sec:
        print('>>> Total timeout reached.')
        self.stop()
