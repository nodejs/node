#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from multiprocessing import Event, Process, Queue

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


def Worker(fn, work_queue, done_queue, done):
  """Worker to be run in a child process.
  The worker stops on two conditions. 1. When the poison pill "STOP" is
  reached or 2. when the event "done" is set."""
  try:
    for args in iter(work_queue.get, "STOP"):
      if done.is_set():
        break
      try:
        done_queue.put(NormalResult(fn(*args)))
      except Exception, e:
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

  def __init__(self, num_workers):
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

  def imap_unordered(self, fn, gen):
    """Maps function "fn" to items in generator "gen" on the worker processes
    in an arbitrary order. The items are expected to be lists of arguments to
    the function. Returns a results iterator."""
    try:
      gen = iter(gen)
      self.advance = self._advance_more

      for w in xrange(self.num_workers):
        p = Process(target=Worker, args=(fn,
                                         self.work_queue,
                                         self.done_queue,
                                         self.done))
        self.processes.append(p)
        p.start()

      self.advance(gen)
      while self.count > 0:
        result = self.done_queue.get()
        self.count -= 1
        if result.exception:
          # Ignore items with unexpected exceptions.
          continue
        elif result.break_now:
          # A keyboard interrupt happened in one of the worker processes.
          raise KeyboardInterrupt
        else:
          yield result.result
        self.advance(gen)
    finally:
      self.terminate()

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
