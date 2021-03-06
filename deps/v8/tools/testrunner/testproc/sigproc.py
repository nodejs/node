# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function

import signal

from . import base
from testrunner.local import utils


class SignalProc(base.TestProcObserver):
  def __init__(self):
    super(SignalProc, self).__init__()
    self.exit_code = utils.EXIT_CODE_PASS

  def setup(self, *args, **kwargs):
    super(SignalProc, self).setup(*args, **kwargs)
    # It should be called after processors are chained together to not loose
    # catched signal.
    signal.signal(signal.SIGINT, self._on_ctrlc)
    signal.signal(signal.SIGTERM, self._on_sigterm)

  def _on_ctrlc(self, _signum, _stack_frame):
    print('>>> Ctrl-C detected, early abort...')
    self.exit_code = utils.EXIT_CODE_INTERRUPTED
    self.stop()

  def _on_sigterm(self, _signum, _stack_frame):
    print('>>> SIGTERM received, early abort...')
    self.exit_code = utils.EXIT_CODE_TERMINATED
    self.stop()
