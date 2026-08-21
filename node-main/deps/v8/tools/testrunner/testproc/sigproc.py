# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import faulthandler
import logging
import signal

from . import base
from testrunner.local import utils


class SignalProc(base.TestProcObserver):
  def __init__(self):
    super(SignalProc, self).__init__()
    self.exit_code = utils.EXIT_CODE_PASS
    signal.signal(signal.SIGINT, self._on_ctrlc)
    signal.signal(signal.SIGTERM, self._on_sigterm)

  def _on_ctrlc(self, _signum, _stack_frame):
    logging.warning('Ctrl-C detected, early abort...')
    self.exit_code = utils.EXIT_CODE_INTERRUPTED
    self.stop()
    if logging.getLogger().isEnabledFor(logging.INFO):
      faulthandler.dump_traceback()

  def _on_sigterm(self, _signum, _stack_frame):
    logging.warning('SIGTERM received, early abort...')
    self.exit_code = utils.EXIT_CODE_TERMINATED
    self.stop()
    if logging.getLogger().isEnabledFor(logging.INFO):
      faulthandler.dump_traceback()

  def worst_exit_code(self, results):
    exit_code = results.exit_code()
    # Indicate if a SIGINT or SIGTERM happened.
    return max(exit_code, self.exit_code)
