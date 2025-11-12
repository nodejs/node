# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from . import base

JobResult = collections.namedtuple('JobResult', ['id', 'result'])

class Job(object):
  def __init__(self, test_id, cmd, outproc, keep_output):
    self.test_id = test_id
    self.cmd = cmd
    self.outproc = outproc
    self.keep_output = keep_output

  def run(self, process_ctx):
    output = self.cmd.execute()
    reduction = process_ctx.result_reduction if not self.keep_output else None
    result = self.outproc.process(output, reduction)
    return JobResult(self.test_id, result)


class ExecutionProc(base.TestProc):
  """Last processor in the chain. Instead of passing tests further it creates
  commands and output processors, executes them in multiple worker processes and
  sends results to the previous processor.
  """

  def __init__(self, ctx, jobs, outproc_factory=None):
    super(ExecutionProc, self).__init__()
    self.ctx = ctx
    self.ctx.pool.init(jobs, notify_function=self.notify_previous)
    self._outproc_factory = outproc_factory or (lambda t: t.output_proc)
    self._tests = {}

  def connect_to(self, next_proc):
    assert False, \
        'ExecutionProc cannot be connected to anything' # pragma: no cover

  def run(self, requirement=None):
    for pool_result in self.ctx.pool.results(requirement):
      self._unpack_result(pool_result)

  def next_test(self, test):
    if self.is_stopped:
      return False

    test_id = test.procid
    cmd = test.get_command(self.ctx)
    self._tests[test_id] = test, cmd

    outproc = self._outproc_factory(test)
    self.ctx.pool.add_jobs([Job(test_id, cmd, outproc, test.keep_output)])

    return True

  def result_for(self, test, result):
    assert False, \
        'ExecutionProc cannot receive results' # pragma: no cover

  def stop(self):
    super(ExecutionProc, self).stop()
    self.ctx.pool.abort()

  def _unpack_result(self, pool_result):
    if pool_result.heartbeat:
      self.heartbeat()
      return

    job_result = pool_result.value
    test_id, result = job_result

    test, result.cmd = self._tests[test_id]
    del self._tests[test_id]
    self._send_result(test, result)
