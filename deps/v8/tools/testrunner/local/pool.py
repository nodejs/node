#!/usr/bin/env python3
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contextlib import contextmanager
from multiprocessing import Process, Queue
import os
import signal
import time
import traceback

try:
  from queue import Empty  # Python 3
except ImportError:
  from Queue import Empty  # Python 2

from . import command
from . import utils


def setup_testing():
  """For testing only: Use threading under the hood instead of multiprocessing
  to make coverage work.
  """
  global Queue
  global Process
  del Queue
  del Process
  try:
    from queue import Queue  # Python 3
  except ImportError:
    from Queue import Queue  # Python 2

  from threading import Thread as Process
  # Monkeypatch threading Queue to look like multiprocessing Queue.
  Queue.cancel_join_thread = lambda self: None
  # Monkeypatch os.kill and add fake pid property on Thread.
  os.kill = lambda *args: None
  Process.pid = property(lambda self: None)


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
  try:
    kwargs = {}
    if process_context_fn and process_context_args is not None:
      kwargs.update(process_context=process_context_fn(*process_context_args))
    for args in iter(work_queue.get, "STOP"):
      try:
        done_queue.put(NormalResult(fn(*args, **kwargs)))
      except command.AbortException:
        # SIGINT, SIGTERM or internal hard timeout.
        break
      except Exception as e:
        traceback.print_exc()
        print(">>> EXCEPTION: %s" % e)
        done_queue.put(ExceptionResult(e))
    # When we reach here on normal tear down, all items have been pulled from
    # the done_queue before and this should have no effect. On fast abort, it's
    # possible that a fast worker left items on the done_queue in memory, which
    # will never be pulled. This call purges those to avoid a deadlock.
    done_queue.cancel_join_thread()
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


class Pool():
  """Distributes tasks to a number of worker processes.
  New tasks can be added dynamically even after the workers have been started.
  Requirement: Tasks can only be added from the parent process, e.g. while
  consuming the results generator."""

  # Factor to calculate the maximum number of items in the work/done queue.
  # Necessary to not overflow the queue's pipe if a keyboard interrupt happens.
  BUFFER_FACTOR = 4

  def __init__(self, num_workers, heartbeat_timeout=1, notify_fun=None):
    """
    Args:
      num_workers: Number of worker processes to run in parallel.
      heartbeat_timeout: Timeout in seconds for waiting for results. Each time
          the timeout is reached, a heartbeat is signalled and timeout is reset.
      notify_fun: Callable called to signale some events like termination. The
          event name is passed as string.
    """
    self.num_workers = num_workers
    self.processes = []
    self.terminated = False
    self.abort_now = False

    # Invariant: processing_count >= #work_queue + #done_queue. It is greater
    # when a worker takes an item from the work_queue and before the result is
    # submitted to the done_queue. It is equal when no worker is working,
    # e.g. when all workers have finished, and when no results are processed.
    # Count is only accessed by the parent process. Only the parent process is
    # allowed to remove items from the done_queue and to add items to the
    # work_queue.
    self.processing_count = 0
    self.heartbeat_timeout = heartbeat_timeout
    self.notify = notify_fun or (lambda x: x)

    # Disable sigint and sigterm to prevent subprocesses from capturing the
    # signals.
    with without_sig():
      self.work_queue = Queue()
      self.done_queue = Queue()

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
    except Exception as e:
      traceback.print_exc()
      print(">>> EXCEPTION: %s" % e)
    finally:
      self._terminate()

    if internal_error:
      raise Exception("Internal error in a worker process.")

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
      if utils.IsWindows():
        command.taskkill_windows(p, verbose=True, force=False)
      else:
        os.kill(p.pid, signal.SIGTERM)

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

    if self.abort_now:
      self._terminate_processes()

    self.notify("Joining workers")
    for p in self.processes:
      p.join()

    # Drain the queues to prevent stderr chatter when queues are garbage
    # collected.
    self.notify("Draining queues")
    try:
      while True: self.work_queue.get(False)
    except:
      pass
    try:
      while True: self.done_queue.get(False)
    except:
      pass

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
