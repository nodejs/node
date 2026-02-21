# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import faulthandler
import logging
import time

from . import base


class TimeoutProc(base.TestProcObserver):
  @staticmethod
  def create(options):
    if not options.total_timeout_sec:
      return None
    return TimeoutProc(options.total_timeout_sec)

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
        logging.info('Total timeout reached.')
        self.stop()
        if logging.getLogger().isEnabledFor(logging.INFO):
          faulthandler.dump_traceback()
