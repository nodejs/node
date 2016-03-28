#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from Queue import Empty
from multiprocessing import Event, Process, Queue
import traceback


class NormalResult():
  def __init__(self, result):
    self.result = result
    self.exception = False
    self.break_now = False


class ExceptionResult():
  def __init__(self):
    self.exception = True
    self.break_now = False


class BreakResult():
  def __init__(self):
    self.exception = False
    self.break_now = True


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


def Worker(fn, work_queue, done_queue, done,
           process_context_fn=None, process_context_args=None):
  """Worker to be run in a child process.
  The worker stops on two conditions. 1. When the poison pill "STOP" is
  reached or 2. when the event "done" is set."""
  try:
    kwargs = {}
    if process_context_fn and process_context_args is not None:
      kwargs.update(process_context=process_context_fn(*process_context_args))
    for args in iter(work_queue.get, "STOP"):
      if done.is_set():
        break
      try:
        done_queue.put(NormalResult(fn(*args, **kwargs)))
      except Exception, e:
        traceback.print_exc()
        print(">>> EXCEPTION: %s" % e)
        done_queue.put(ExceptionResult())
  except KeyboardInterrupt:
    done_queue.put(BreakResult())


class Pool():
  """Distributes tasks to a number of worker processes.
  New tasks can be added dynamically even after the workers have been started.
  Requirement: Tasks can only be added from the parent process, e.g. while
  consuming the results generator."""

  # Factor to calculate the maximum number of items in the work/done queue.
  # Necessary to not overflow the queue's pipe if a keyboard interrupt happens.
  BUFFER_FACTOR = 4

  def __init__(self, num_workers, heartbeat_timeout=30):
    self.num_workers = num_workers
    self.processes = []
    self.terminated = False

    # Invariant: count >= #work_queue + #done_queue. It is greater when a
    # worker takes an item from the work_queue and before the result is
    # submitted to the done_queue. It is equal when no worker is working,
    # e.g. when all workers have finished, and when no results are processed.
    # Count is only accessed by the parent process. Only the parent process is
    # allowed to remove items from the done_queue and to add items to the
    # work_queue.
    self.count = 0
    self.work_queue = Queue()
    self.done_queue = Queue()
    self.done = Event()
    self.heartbeat_timeout = heartbeat_timeout

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
    try:
      internal_error = False
      gen = iter(gen)
      self.advance = self._advance_more

      for w in xrange(self.num_workers):
        p = Process(target=Worker, args=(fn,
                                         self.work_queue,
                                         self.done_queue,
                                         self.done,
                                         process_context_fn,
                                         process_context_args))
        self.processes.append(p)
        p.start()

      self.advance(gen)
      while self.count > 0:
        while True:
          try:
            result = self.done_queue.get(timeout=self.heartbeat_timeout)
            break
          except Empty:
            # Indicate a heartbeat. The iterator will continue fetching the
            # next result.
            yield MaybeResult.create_heartbeat()
        self.count -= 1
        if result.exception:
          # TODO(machenbach): Handle a few known types of internal errors
          # gracefully, e.g. missing test files.
          internal_error = True
          continue
        elif result.break_now:
          # A keyboard interrupt happened in one of the worker processes.
          raise KeyboardInterrupt
        else:
          yield MaybeResult.create_result(result.result)
        self.advance(gen)
    finally:
      self.terminate()
    if internal_error:
      raise Exception("Internal error in a worker process.")

  def _advance_more(self, gen):
    while self.count < self.num_workers * self.BUFFER_FACTOR:
      try:
        self.work_queue.put(gen.next())
        self.count += 1
      except StopIteration:
        self.advance = self._advance_empty
        break

  def _advance_empty(self, gen):
    pass

  def add(self, args):
    """Adds an item to the work queue. Can be called dynamically while
    processing the results from imap_unordered."""
    self.work_queue.put(args)
    self.count += 1

  def terminate(self):
    if self.terminated:
      return
    self.terminated = True

    # For exceptional tear down set the "done" event to stop the workers before
    # they empty the queue buffer.
    self.done.set()

    for p in self.processes:
      # During normal tear down the workers block on get(). Feed a poison pill
      # per worker to make them stop.
      self.work_queue.put("STOP")

    for p in self.processes:
      p.join()

    # Drain the queues to prevent failures when queues are garbage collected.
    try:
      while True: self.work_queue.get(False)
    except:
      pass
    try:
      while True: self.done_queue.get(False)
    except:
      pass
