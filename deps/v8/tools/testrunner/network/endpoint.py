# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import multiprocessing
import os
import Queue
import threading
import time

from ..local import execution
from ..local import progress
from ..local import testsuite
from ..local import utils
from ..server import compression


class EndpointProgress(progress.ProgressIndicator):
  def __init__(self, sock, server, ctx):
    super(EndpointProgress, self).__init__()
    self.sock = sock
    self.server = server
    self.context = ctx
    self.results_queue = []  # Accessors must synchronize themselves.
    self.sender_lock = threading.Lock()
    self.senderthread = threading.Thread(target=self._SenderThread)
    self.senderthread.start()

  def HasRun(self, test, has_unexpected_output):
    # The runners that call this have a lock anyway, so this is safe.
    self.results_queue.append(test)

  def _SenderThread(self):
    keep_running = True
    tests = []
    self.sender_lock.acquire()
    while keep_running:
      time.sleep(0.1)
      # This should be "atomic enough" without locking :-)
      # (We don't care which list any new elements get appended to, as long
      # as we don't lose any and the last one comes last.)
      current = self.results_queue
      self.results_queue = []
      for c in current:
        if c is None:
          keep_running = False
        else:
          tests.append(c)
      if keep_running and len(tests) < 1:
        continue  # Wait for more results.
      if len(tests) < 1: break  # We're done here.
      result = []
      for t in tests:
        result.append(t.PackResult())
      try:
        compression.Send(result, self.sock)
      except:
        self.runner.terminate = True
      for t in tests:
        self.server.CompareOwnPerf(t, self.context.arch, self.context.mode)
      tests = []
    self.sender_lock.release()


def Execute(workspace, ctx, tests, sock, server):
  suite_paths = utils.GetSuitePaths(os.path.join(workspace, "test"))
  suites = []
  for root in suite_paths:
    suite = testsuite.TestSuite.LoadTestSuite(
        os.path.join(workspace, "test", root))
    if suite:
      suite.SetupWorkingDirectory()
      suites.append(suite)

  suites_dict = {}
  for s in suites:
    suites_dict[s.name] = s
    s.tests = []
  for t in tests:
    suite = suites_dict[t.suite]
    t.suite = suite
    suite.tests.append(t)

  suites = [ s for s in suites if len(s.tests) > 0 ]
  for s in suites:
    s.DownloadData()

  progress_indicator = EndpointProgress(sock, server, ctx)
  runner = execution.Runner(suites, progress_indicator, ctx)
  try:
    runner.Run(server.jobs)
  except IOError, e:
    if e.errno == 2:
      message = ("File not found: %s, maybe you forgot to 'git add' it?" %
                 e.filename)
    else:
      message = "%s" % e
    compression.Send([[-1, message]], sock)
  progress_indicator.HasRun(None, None)  # Sentinel to signal the end.
  progress_indicator.sender_lock.acquire()  # Released when sending is done.
  progress_indicator.sender_lock.release()
