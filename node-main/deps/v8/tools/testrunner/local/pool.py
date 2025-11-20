#!/usr/bin/env python3
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import os
import signal
import threading
import traceback

from contextlib import contextmanager
from multiprocessing import Process, Queue
from queue import Empty



def setup_testing():
  """For testing only: Use threading under the hood instead of multiprocessing
  to make coverage work.
  """
  global Queue
  global Process
  del Queue
  del Process
  from queue import Queue
  from threading import Thread as Process
  # Monkeypatch os.kill and add fake pid property on Thread.
  os.kill = lambda *args: None
  Process.pid = property(lambda self: None)


class AbortException(Exception):
  """Indicates early abort on SIGINT, SIGTERM or internal hard timeout."""
  pass


class NormalResult():
  def __init__(self, result):
    self.result = result
    self.exception = None

class ExceptionResult():
  def __init__(self, exception):
    self.exception = exception


class MaybeResult():
  def __init__(self, heartbeat, value):
    self.heartbeat = heartbeat
    self.value = value

  @staticmethod
  def create_heartbeat():
    return MaybeResult(True, None)

  @staticmethod
  def create_result(value):
    return MaybeResult(False, value)


def Worker(fn, work_queue, done_queue,
           process_context_fn=None, process_context_args=None):
  """Worker to be run in a child process.
  The worker stops when the poison pill "STOP" is reached.
  """
  # Install a default signal handler for SIGTERM that stops the processing
  # loop below on the next occasion. The job function "fn" is supposed to
  # register their own handler to avoid blocking, but still chain to this
  # handler on SIGTERM to terminate the loop quickly.
  stop = [False]
  def handler(signum, frame):
    stop[0] = True
  signal.signal(signal.SIGTERM, handler)

  try:
    kwargs = {}
    if process_context_fn and process_context_args is not None:
      kwargs.update(process_context=process_context_fn(*process_context_args))
    for args in iter(work_queue.get, "STOP"):
      if stop[0]:
        # SIGINT, SIGTERM or internal hard timeout caught outside the execution
        # of "fn".
        break
      try:
        done_queue.put(NormalResult(fn(*args, **kwargs)))
      except AbortException:
        # SIGINT, SIGTERM or internal hard timeout caught during execution of
        # "fn".
        break
      except Exception as e:
        logging.exception('Unhandled error during worker execution.')
        done_queue.put(ExceptionResult(e))
  except KeyboardInterrupt:
    assert False, 'Unreachable'


@contextmanager
def without_sig():
  int_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
  term_handler = signal.signal(signal.SIGTERM, signal.SIG_IGN)
  try:
    yield
  finally:
    signal.signal(signal.SIGINT, int_handler)
    signal.signal(signal.SIGTERM, term_handler)


@contextmanager
def drain_queue_async(queue):
  """Drains a queue in a background thread until the wrapped code unblocks.

  This can be used to unblock joining a child process that might still write
  to the queue. The join should be wrapped by this context manager.
  """
  keep_running = True

  def empty_queue():
    elem_count = 0
    while keep_running:
      try:
        while True:
          queue.get(True, 0.1)
          elem_count += 1
          if elem_count < 200:
            logging.info('Drained an element from queue.')
      except Empty:
        pass
      except:
        logging.exception('Error draining queue.')

  emptier = threading.Thread(target=empty_queue)
  emptier.start()
  yield
  keep_running = False
  emptier.join()


class ContextPool():

  def __init__(self):
    self.abort_now = False

  def init(self, num_workers, heartbeat_timeout=1, notify_function=None):
    """
    Delayed initialization. At context creation time we have no access to the
    below described parameters.
    Args:
      num_workers: Number of worker processes to run in parallel.
      heartbeat_timeout: Timeout in seconds for waiting for results. Each time
          the timeout is reached, a heartbeat is signalled and timeout is reset.
      notify_function: Callable called to signal some events like termination. The
          event name is passed as string.
    """
    pass

  def add_jobs(self, jobs):
    pass

  def results(self, requirement):
    pass

  def abort(self):
    self.abort_now = True


ProcessContext = collections.namedtuple('ProcessContext', ['result_reduction'])


class DefaultExecutionPool(ContextPool):
  """Distributes tasks to a number of worker processes.
  New tasks can be added dynamically even after the workers have been started.
  Requirement: Tasks can only be added from the parent process, e.g. while
  consuming the results generator."""

  # Factor to calculate the maximum number of items in the work/done queue.
  # Necessary to not overflow the queue's pipe if a keyboard interrupt happens.
  BUFFER_FACTOR = 4

  def __init__(self, os_context=None):
    super(DefaultExecutionPool, self).__init__()
    self.os_context = os_context
    self.processes = []
    self.terminated = False

    # Invariant: processing_count >= #work_queue + #done_queue. It is greater
    # when a worker takes an item from the work_queue and before the result is
    # submitted to the done_queue. It is equal when no worker is working,
    # e.g. when all workers have finished, and when no results are processed.
    # Count is only accessed by the parent process. Only the parent process is
    # allowed to remove items from the done_queue and to add items to the
    # work_queue.
    self.processing_count = 0

    # Disable sigint and sigterm to prevent subprocesses from capturing the
    # signals.
    with without_sig():
      self.work_queue = Queue()
      self.done_queue = Queue()

  def init(self, num_workers=1, heartbeat_timeout=1, notify_function=None):
    """
    Args:
      num_workers: Number of worker processes to run in parallel.
      heartbeat_timeout: Timeout in seconds for waiting for results. Each time
          the timeout is reached, a heartbeat is signalled and timeout is reset.
      notify_function: Callable called to signal some events like termination. The
          event name is passed as string.
    """
    self.num_workers = num_workers
    self.heartbeat_timeout = heartbeat_timeout
    self.notify = notify_function or (lambda x: x)

  def add_jobs(self, jobs):
    self.add(jobs)

  def results(self, requirement):
    return self.imap_unordered(
        fn=run_job,
        gen=[],
        process_context_fn=ProcessContext,
        process_context_args=[requirement],
    )

  def imap_unordered(self, fn, gen,
                     process_context_fn=None, process_context_args=None):
    """Maps function "fn" to items in generator "gen" on the worker processes
    in an arbitrary order. The items are expected to be lists of arguments to
    the function. Returns a results iterator. A result value of type
    MaybeResult either indicates a heartbeat of the runner, i.e. indicating
    that the runner is still waiting for the result to be computed, or it wraps
    the real result.

    Args:
      process_context_fn: Function executed once by each worker. Expected to
          return a process-context object. If present, this object is passed
          as additional argument to each call to fn.
      process_context_args: List of arguments for the invocation of
          process_context_fn. All arguments will be pickled and sent beyond the
          process boundary.
    """
    if self.terminated:
      return
    try:
      internal_error = False
      gen = iter(gen)
      self.advance = self._advance_more

      # Disable sigint and sigterm to prevent subprocesses from capturing the
      # signals.
      with without_sig():
        for w in range(self.num_workers):
          p = Process(target=Worker, args=(fn,
                                          self.work_queue,
                                          self.done_queue,
                                          process_context_fn,
                                          process_context_args))
          p.start()
          self.processes.append(p)

      self.advance(gen)
      while self.processing_count > 0:
        while True:
          try:
            # Read from result queue in a responsive fashion. If available,
            # this will return a normal result immediately or a heartbeat on
            # heartbeat timeout (default 1 second).
            result = self._get_result_from_queue()
          except:
            # TODO(machenbach): Handle a few known types of internal errors
            # gracefully, e.g. missing test files.
            logging.exception('Internal error in a worker process.')
            internal_error = True
            continue
          finally:
            if self.abort_now:
              # SIGINT, SIGTERM or internal hard timeout.
              return

          yield result
          break

        self.advance(gen)
    except KeyboardInterrupt:
      assert False, 'Unreachable'
    except Exception:
      logging.exception('Unhandled error during pool execution.')
    finally:
      self._terminate()

    if internal_error:
      raise Exception('Internal error in a worker process.')

  def _advance_more(self, gen):
    while self.processing_count < self.num_workers * self.BUFFER_FACTOR:
      try:
        self.work_queue.put(next(gen))
        self.processing_count += 1
      except StopIteration:
        self.advance = self._advance_empty
        break

  def _advance_empty(self, gen):
    pass

  def add(self, args):
    """Adds an item to the work queue. Can be called dynamically while
    processing the results from imap_unordered."""
    assert not self.terminated

    self.work_queue.put(args)
    self.processing_count += 1

  def abort(self):
    """Schedules abort on next queue read.

    This is safe to call when handling SIGINT, SIGTERM or when an internal
    hard timeout is reached.
    """
    self.abort_now = True

  def _terminate_processes(self):
    for p in self.processes:
      self.os_context.terminate_process(p)

  def _terminate(self):
    """Terminates execution and cleans up the queues.

    If abort() was called before termination, this also terminates the
    subprocesses and doesn't wait for ongoing tests.
    """
    if self.terminated:
      return
    self.terminated = True

    # Drain out work queue from tests
    try:
      while True:
        self.work_queue.get(True, 0.1)
    except Empty:
      pass

    # Make sure all processes stop
    for _ in self.processes:
      # During normal tear down the workers block on get(). Feed a poison pill
      # per worker to make them stop.
      self.work_queue.put("STOP")

    # Send a SIGTERM to all workers. They will gracefully terminate their
    # processing loop and if the signal is caught during job execution they
    # will try to terminate the ongoing test processes quickly.
    if self.abort_now:
      self._terminate_processes()

    self.notify("Joining workers")
    with drain_queue_async(self.done_queue):
      for p in self.processes:
        p.join()

    self.notify("Pool terminated")

  def _get_result_from_queue(self):
    """Attempts to get the next result from the queue.

    Returns: A wrapped result if one was available within heartbeat timeout,
        a heartbeat result otherwise.
    Raises:
        Exception: If an exception occured when processing the task on the
            worker side, it is reraised here.
    """
    while True:
      try:
        result = self.done_queue.get(timeout=self.heartbeat_timeout)
        self.processing_count -= 1
        if result.exception:
          raise result.exception
        return MaybeResult.create_result(result.result)
      except Empty:
        return MaybeResult.create_heartbeat()


class SingleThreadedExecutionPool(ContextPool):

  def __init__(self):
    super(SingleThreadedExecutionPool, self).__init__()
    self.work_queue = []

  def add_jobs(self, jobs):
    self.work_queue.extend(jobs)

  def results(self, requirement):
    while self.work_queue and not self.abort_now:
      job = self.work_queue.pop()
      yield MaybeResult.create_result(job.run(ProcessContext(requirement)))


# Global function for multiprocessing, because pickling a static method doesn't
# work on Windows.
def run_job(job, process_context):
  return job.run(process_context)
