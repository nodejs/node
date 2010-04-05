#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005-2008 (ita)

"Execute the tasks"

import sys, random, time, threading, traceback
try: from Queue import Queue
except ImportError: from queue import Queue
import Build, Utils, Logs, Options
from Logs import debug, error
from Constants import *

GAP = 15

run_old = threading.Thread.run
def run(*args, **kwargs):
	try:
		run_old(*args, **kwargs)
	except (KeyboardInterrupt, SystemExit):
		raise
	except:
		sys.excepthook(*sys.exc_info())
threading.Thread.run = run

class TaskConsumer(threading.Thread):
	ready = Queue(0)
	consumers = []

	def __init__(self):
		threading.Thread.__init__(self)
		self.setDaemon(1)
		self.start()

	def run(self):
		try:
			self.loop()
		except:
			pass

	def loop(self):
		while 1:
			tsk = TaskConsumer.ready.get()
			m = tsk.master
			if m.stop:
				m.out.put(tsk)
				continue

			try:
				tsk.generator.bld.printout(tsk.display())
				if tsk.__class__.stat: ret = tsk.__class__.stat(tsk)
				# actual call to task's run() function
				else: ret = tsk.call_run()
			except Exception, e:
				tsk.err_msg = Utils.ex_stack()
				tsk.hasrun = EXCEPTION

				# TODO cleanup
				m.error_handler(tsk)
				m.out.put(tsk)
				continue

			if ret:
				tsk.err_code = ret
				tsk.hasrun = CRASHED
			else:
				try:
					tsk.post_run()
				except Utils.WafError:
					pass
				except Exception:
					tsk.err_msg = Utils.ex_stack()
					tsk.hasrun = EXCEPTION
				else:
					tsk.hasrun = SUCCESS
			if tsk.hasrun != SUCCESS:
				m.error_handler(tsk)

			m.out.put(tsk)

class Parallel(object):
	"""
	keep the consumer threads busy, and avoid consuming cpu cycles
	when no more tasks can be added (end of the build, etc)
	"""
	def __init__(self, bld, j=2):

		# number of consumers
		self.numjobs = j

		self.manager = bld.task_manager
		self.manager.current_group = 0

		self.total = self.manager.total()

		# tasks waiting to be processed - IMPORTANT
		self.outstanding = []
		self.maxjobs = MAXJOBS

		# tasks that are awaiting for another task to complete
		self.frozen = []

		# tasks returned by the consumers
		self.out = Queue(0)

		self.count = 0 # tasks not in the producer area

		self.processed = 1 # progress indicator

		self.stop = False # error condition to stop the build
		self.error = False # error flag

	def get_next(self):
		"override this method to schedule the tasks in a particular order"
		if not self.outstanding:
			return None
		return self.outstanding.pop(0)

	def postpone(self, tsk):
		"override this method to schedule the tasks in a particular order"
		# TODO consider using a deque instead
		if random.randint(0, 1):
			self.frozen.insert(0, tsk)
		else:
			self.frozen.append(tsk)

	def refill_task_list(self):
		"called to set the next group of tasks"

		while self.count > self.numjobs + GAP or self.count >= self.maxjobs:
			self.get_out()

		while not self.outstanding:
			if self.count:
				self.get_out()

			if self.frozen:
				self.outstanding += self.frozen
				self.frozen = []
			elif not self.count:
				(jobs, tmp) = self.manager.get_next_set()
				if jobs != None: self.maxjobs = jobs
				if tmp: self.outstanding += tmp
				break

	def get_out(self):
		"the tasks that are put to execute are all collected using get_out"
		ret = self.out.get()
		self.manager.add_finished(ret)
		if not self.stop and getattr(ret, 'more_tasks', None):
			self.outstanding += ret.more_tasks
			self.total += len(ret.more_tasks)
		self.count -= 1

	def error_handler(self, tsk):
		"by default, errors make the build stop (not thread safe so be careful)"
		if not Options.options.keep:
			self.stop = True
		self.error = True

	def start(self):
		"execute the tasks"

		if TaskConsumer.consumers:
			# the worker pool is usually loaded lazily (see below)
			# in case it is re-used with a different value of numjobs:
			while len(TaskConsumer.consumers) < self.numjobs:
				TaskConsumer.consumers.append(TaskConsumer())

		while not self.stop:

			self.refill_task_list()

			# consider the next task
			tsk = self.get_next()
			if not tsk:
				if self.count:
					# tasks may add new ones after they are run
					continue
				else:
					# no tasks to run, no tasks running, time to exit
					break

			if tsk.hasrun:
				# if the task is marked as "run", just skip it
				self.processed += 1
				self.manager.add_finished(tsk)
				continue

			try:
				st = tsk.runnable_status()
			except Exception, e:
				self.processed += 1
				if self.stop and not Options.options.keep:
					tsk.hasrun = SKIPPED
					self.manager.add_finished(tsk)
					continue
				self.error_handler(tsk)
				self.manager.add_finished(tsk)
				tsk.hasrun = EXCEPTION
				tsk.err_msg = Utils.ex_stack()
				continue

			if st == ASK_LATER:
				self.postpone(tsk)
			elif st == SKIP_ME:
				self.processed += 1
				tsk.hasrun = SKIPPED
				self.manager.add_finished(tsk)
			else:
				# run me: put the task in ready queue
				tsk.position = (self.processed, self.total)
				self.count += 1
				tsk.master = self
				TaskConsumer.ready.put(tsk)
				self.processed += 1

				# create the consumer threads only if there is something to consume
				if not TaskConsumer.consumers:
					TaskConsumer.consumers = [TaskConsumer() for i in xrange(self.numjobs)]

		# self.count represents the tasks that have been made available to the consumer threads
		# collect all the tasks after an error else the message may be incomplete
		while self.error and self.count:
			self.get_out()

		#print loop
		assert (self.count == 0 or self.stop)

