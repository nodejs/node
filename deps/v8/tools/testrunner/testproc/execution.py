# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import traceback

from . import base
from ..local import pool


# Global function for multiprocessing, because pickling a static method doesn't
# work on Windows.
def run_job(job, process_context):
  return job.run(process_context)


def create_process_context(result_reduction):
  return ProcessContext(result_reduction)


JobResult = collections.namedtuple('JobResult', ['id', 'result'])
ProcessContext = collections.namedtuple('ProcessContext', ['result_reduction'])


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


# TODO(majeski): Stop workers when processor is stopped. It will also require
# to call stop both directions from TimeoutProc.
class ExecutionProc(base.TestProc):
  """Last processor in the chain. Instead of passing tests further it creates
  commands and output processors, executes them in multiple worker processes and
  sends results to the previous processor.
  """

  def __init__(self, jobs, context, outproc_factory=None):
    super(ExecutionProc, self).__init__()
    self._pool = pool.Pool(jobs)
    self._context = context
    self._outproc_factory = outproc_factory or (lambda t: t.output_proc)
    self._tests = {}

  def connect_to(self, next_proc):
    assert False, 'ExecutionProc cannot be connected to anything'

  def start(self):
    try:
      it = self._pool.imap_unordered(
        fn=run_job,
        gen=[],
        process_context_fn=create_process_context,
        process_context_args=[self._prev_requirement],
      )
      for pool_result in it:
        if pool_result.heartbeat:
          continue

        job_result = pool_result.value
        test_id, result = job_result

        test, result.cmd = self._tests[test_id]
        del self._tests[test_id]
        self._send_result(test, result)
    except KeyboardInterrupt:
      raise
    except:
      traceback.print_exc()
      raise
    finally:
      self._pool.terminate()

  def next_test(self, test):
    test_id = test.procid
    cmd = test.get_command(self._context)
    self._tests[test_id] = test, cmd

    outproc = self._outproc_factory(test)
    self._pool.add([Job(test_id, cmd, outproc, test.keep_output)])

  def result_for(self, test, result):
    assert False, 'ExecutionProc cannot receive results'
