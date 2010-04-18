#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005-2008 (ita)

"""
Running tasks in parallel is a simple problem, but in practice it is more complicated:
* dependencies discovered during the build (dynamic task creation)
* dependencies discovered after files are compiled
* the amount of tasks and dependencies (graph size) can be huge

This is why the dependency management is split on three different levels:
1. groups of tasks that run all after another group of tasks
2. groups of tasks that can be run in parallel
3. tasks that can run in parallel, but with possible unknown ad-hoc dependencies

The point #1 represents a strict sequential order between groups of tasks, for example a compiler is produced
and used to compile the rest, whereas #2 and #3 represent partial order constraints where #2 applies to the kind of task
and #3 applies to the task instances.

#1 is held by the task manager: ordered list of TaskGroups (see bld.add_group)
#2 is held by the task groups and the task types: precedence after/before (topological sort),
   and the constraints extracted from file extensions
#3 is held by the tasks individually (attribute run_after),
   and the scheduler (Runner.py) use Task::runnable_status to reorder the tasks

--

To try, use something like this in your code:
import Constants, Task
Task.algotype = Constants.MAXPARALLEL

--

There are two concepts with the tasks (individual units of change):
* dependency (if 1 is recompiled, recompile 2)
* order (run 2 after 1)

example 1: if t1 depends on t2 and t2 depends on t3 it is not necessary to make t1 depend on t3 (dependency is transitive)
example 2: if t1 depends on a node produced by t2, it is not immediately obvious that t1 must run after t2 (order is not obvious)

The role of the Task Manager is to give the tasks in order (groups of task that may be run in parallel one after the other)

"""

import os, shutil, sys, re, random, datetime, tempfile, shlex
from Utils import md5
import Build, Runner, Utils, Node, Logs, Options
from Logs import debug, warn, error
from Constants import *

algotype = NORMAL
#algotype = JOBCONTROL
#algotype = MAXPARALLEL

COMPILE_TEMPLATE_SHELL = '''
def f(task):
	env = task.env
	wd = getattr(task, 'cwd', None)
	p = env.get_flat
	cmd = \'\'\' %s \'\'\' % s
	return task.exec_command(cmd, cwd=wd)
'''

COMPILE_TEMPLATE_NOSHELL = '''
def f(task):
	env = task.env
	wd = getattr(task, 'cwd', None)
	def to_list(xx):
		if isinstance(xx, str): return [xx]
		return xx
	lst = []
	%s
	lst = [x for x in lst if x]
	return task.exec_command(lst, cwd=wd)
'''


"""
Enable different kind of dependency algorithms:
1 make groups: first compile all cpps and then compile all links (NORMAL)
2 parallelize all (each link task run after its dependencies) (MAXPARALLEL)
3 like 1 but provide additional constraints for the parallelization (MAXJOBS)

In theory 1. will be faster than 2 for waf, but might be slower for builds
The scheme 2 will not allow for running tasks one by one so it can cause disk thrashing on huge builds
"""

file_deps = Utils.nada
"""
Additional dependency pre-check may be added by replacing the function file_deps.
e.g. extract_outputs, extract_deps below.
"""

class TaskManager(object):
	"""The manager is attached to the build object, it holds a list of TaskGroup"""
	def __init__(self):
		self.groups = []
		self.tasks_done = []
		self.current_group = 0
		self.groups_names = {}

	def get_next_set(self):
		"""return the next set of tasks to execute
		the first parameter is the maximum amount of parallelization that may occur"""
		ret = None
		while not ret and self.current_group < len(self.groups):
			ret = self.groups[self.current_group].get_next_set()
			if ret: return ret
			else:
				self.groups[self.current_group].process_install()
				self.current_group += 1
		return (None, None)

	def add_group(self, name=None, set=True):
		#if self.groups and not self.groups[0].tasks:
		#	error('add_group: an empty group is already present')
		g = TaskGroup()

		if name and name in self.groups_names:
			error('add_group: name %s already present' % name)
		self.groups_names[name] = g
		self.groups.append(g)
		if set:
			self.current_group = len(self.groups) - 1

	def set_group(self, idx):
		if isinstance(idx, str):
			g = self.groups_names[idx]
			for x in xrange(len(self.groups)):
				if id(g) == id(self.groups[x]):
					self.current_group = x
		else:
			self.current_group = idx

	def add_task_gen(self, tgen):
		if not self.groups: self.add_group()
		self.groups[self.current_group].tasks_gen.append(tgen)

	def add_task(self, task):
		if not self.groups: self.add_group()
		self.groups[self.current_group].tasks.append(task)

	def total(self):
		total = 0
		if not self.groups: return 0
		for group in self.groups:
			total += len(group.tasks)
		return total

	def add_finished(self, tsk):
		self.tasks_done.append(tsk)
		bld = tsk.generator.bld
		if bld.is_install:
			f = None
			if 'install' in tsk.__dict__:
				f = tsk.__dict__['install']
				# install=0 to prevent installation
				if f: f(tsk)
			else:
				tsk.install()

class TaskGroup(object):
	"the compilation of one group does not begin until the previous group has finished (in the manager)"
	def __init__(self):
		self.tasks = [] # this list will be consumed
		self.tasks_gen = []

		self.cstr_groups = Utils.DefaultDict(list) # tasks having equivalent constraints
		self.cstr_order = Utils.DefaultDict(set) # partial order between the cstr groups
		self.temp_tasks = [] # tasks put on hold
		self.ready = 0
		self.post_funs = []

	def reset(self):
		"clears the state of the object (put back the tasks into self.tasks)"
		for x in self.cstr_groups:
			self.tasks += self.cstr_groups[x]
		self.tasks = self.temp_tasks + self.tasks
		self.temp_tasks = []
		self.cstr_groups = Utils.DefaultDict(list)
		self.cstr_order = Utils.DefaultDict(set)
		self.ready = 0

	def process_install(self):
		for (f, k, kw) in self.post_funs:
			f(*k, **kw)

	def prepare(self):
		"prepare the scheduling"
		self.ready = 1
		file_deps(self.tasks)
		self.make_cstr_groups()
		self.extract_constraints()

	def get_next_set(self):
		"next list of tasks to execute using max job settings, returns (maxjobs, task_list)"
		global algotype
		if algotype == NORMAL:
			tasks = self.tasks_in_parallel()
			maxj = MAXJOBS
		elif algotype == JOBCONTROL:
			(maxj, tasks) = self.tasks_by_max_jobs()
		elif algotype == MAXPARALLEL:
			tasks = self.tasks_with_inner_constraints()
			maxj = MAXJOBS
		else:
			raise Utils.WafError("unknown algorithm type %s" % (algotype))

		if not tasks: return ()
		return (maxj, tasks)

	def make_cstr_groups(self):
		"unite the tasks that have similar constraints"
		self.cstr_groups = Utils.DefaultDict(list)
		for x in self.tasks:
			h = x.hash_constraints()
			self.cstr_groups[h].append(x)

	def set_order(self, a, b):
		self.cstr_order[a].add(b)

	def compare_exts(self, t1, t2):
		"extension production"
		x = "ext_in"
		y = "ext_out"
		in_ = t1.attr(x, ())
		out_ = t2.attr(y, ())
		for k in in_:
			if k in out_:
				return -1
		in_ = t2.attr(x, ())
		out_ = t1.attr(y, ())
		for k in in_:
			if k in out_:
				return 1
		return 0

	def compare_partial(self, t1, t2):
		"partial relations after/before"
		m = "after"
		n = "before"
		name = t2.__class__.__name__
		if name in Utils.to_list(t1.attr(m, ())): return -1
		elif name in Utils.to_list(t1.attr(n, ())): return 1
		name = t1.__class__.__name__
		if name in Utils.to_list(t2.attr(m, ())): return 1
		elif name in Utils.to_list(t2.attr(n, ())): return -1
		return 0

	def extract_constraints(self):
		"extract the parallelization constraints from the tasks with different constraints"
		keys = self.cstr_groups.keys()
		max = len(keys)
		# hopefully the length of this list is short
		for i in xrange(max):
			t1 = self.cstr_groups[keys[i]][0]
			for j in xrange(i + 1, max):
				t2 = self.cstr_groups[keys[j]][0]

				# add the constraints based on the comparisons
				val = (self.compare_exts(t1, t2)
					or self.compare_partial(t1, t2)
					)
				if val > 0:
					self.set_order(keys[i], keys[j])
				elif val < 0:
					self.set_order(keys[j], keys[i])

	def tasks_in_parallel(self):
		"(NORMAL) next list of tasks that may be executed in parallel"

		if not self.ready: self.prepare()

		keys = self.cstr_groups.keys()

		unconnected = []
		remainder = []

		for u in keys:
			for k in self.cstr_order.values():
				if u in k:
					remainder.append(u)
					break
			else:
				unconnected.append(u)

		toreturn = []
		for y in unconnected:
			toreturn.extend(self.cstr_groups[y])

		# remove stuff only after
		for y in unconnected:
				try: self.cstr_order.__delitem__(y)
				except KeyError: pass
				self.cstr_groups.__delitem__(y)

		if not toreturn and remainder:
			raise Utils.WafError("circular order constraint detected %r" % remainder)

		return toreturn

	def tasks_by_max_jobs(self):
		"(JOBCONTROL) returns the tasks that can run in parallel with the max amount of jobs"
		if not self.ready: self.prepare()
		if not self.temp_tasks: self.temp_tasks = self.tasks_in_parallel()
		if not self.temp_tasks: return (None, None)

		maxjobs = MAXJOBS
		ret = []
		remaining = []
		for t in self.temp_tasks:
			m = getattr(t, "maxjobs", getattr(self.__class__, "maxjobs", MAXJOBS))
			if m > maxjobs:
				remaining.append(t)
			elif m < maxjobs:
				remaining += ret
				ret = [t]
				maxjobs = m
			else:
				ret.append(t)
		self.temp_tasks = remaining
		return (maxjobs, ret)

	def tasks_with_inner_constraints(self):
		"""(MAXPARALLEL) returns all tasks in this group, but add the constraints on each task instance
		as an optimization, it might be desirable to discard the tasks which do not have to run"""
		if not self.ready: self.prepare()

		if getattr(self, "done", None): return None

		for p in self.cstr_order:
			for v in self.cstr_order[p]:
				for m in self.cstr_groups[p]:
					for n in self.cstr_groups[v]:
						n.set_run_after(m)
		self.cstr_order = Utils.DefaultDict(set)
		self.cstr_groups = Utils.DefaultDict(list)
		self.done = 1
		return self.tasks[:] # make a copy

class store_task_type(type):
	"store the task types that have a name ending in _task into a map (remember the existing task types)"
	def __init__(cls, name, bases, dict):
		super(store_task_type, cls).__init__(name, bases, dict)
		name = cls.__name__

		if name.endswith('_task'):
			name = name.replace('_task', '')
		if name != 'TaskBase':
			TaskBase.classes[name] = cls

class TaskBase(object):
	"""Base class for all Waf tasks

	The most important methods are (by usual order of call):
	1 runnable_status: ask the task if it should be run, skipped, or if we have to ask later
	2 __str__: string to display to the user
	3 run: execute the task
	4 post_run: after the task is run, update the cache about the task

	This class should be seen as an interface, it provides the very minimum necessary for the scheduler
	so it does not do much.

	For illustration purposes, TaskBase instances try to execute self.fun (if provided)
	"""

	__metaclass__ = store_task_type

	color = "GREEN"
	maxjobs = MAXJOBS
	classes = {}
	stat = None

	def __init__(self, *k, **kw):
		self.hasrun = NOT_RUN

		try:
			self.generator = kw['generator']
		except KeyError:
			self.generator = self
			self.bld = Build.bld

		if kw.get('normal', 1):
			self.generator.bld.task_manager.add_task(self)

	def __repr__(self):
		"used for debugging"
		return '\n\t{task: %s %s}' % (self.__class__.__name__, str(getattr(self, "fun", "")))

	def __str__(self):
		"string to display to the user"
		if hasattr(self, 'fun'):
			return 'executing: %s\n' % self.fun.__name__
		return self.__class__.__name__ + '\n'

	def exec_command(self, *k, **kw):
		"use this for executing commands from tasks"
		# TODO in waf 1.6, eliminate bld.exec_command, and move the cwd processing to here
		if self.env['env']:
			kw['env'] = self.env['env']
		return self.generator.bld.exec_command(*k, **kw)

	def runnable_status(self):
		"RUN_ME SKIP_ME or ASK_LATER"
		return RUN_ME

	def can_retrieve_cache(self):
		return False

	def call_run(self):
		if self.can_retrieve_cache():
			return 0
		return self.run()

	def run(self):
		"called if the task must run"
		if hasattr(self, 'fun'):
			return self.fun(self)
		return 0

	def post_run(self):
		"update the dependency tree (node stats)"
		pass

	def display(self):
		"print either the description (using __str__) or the progress bar or the ide output"
		col1 = Logs.colors(self.color)
		col2 = Logs.colors.NORMAL

		if Options.options.progress_bar == 1:
			return self.generator.bld.progress_line(self.position[0], self.position[1], col1, col2)

		if Options.options.progress_bar == 2:
			ela = Utils.get_elapsed_time(self.generator.bld.ini)
			try:
				ins  = ','.join([n.name for n in self.inputs])
			except AttributeError:
				ins = ''
			try:
				outs = ','.join([n.name for n in self.outputs])
			except AttributeError:
				outs = ''
			return '|Total %s|Current %s|Inputs %s|Outputs %s|Time %s|\n' % (self.position[1], self.position[0], ins, outs, ela)

		total = self.position[1]
		n = len(str(total))
		fs = '[%%%dd/%%%dd] %%s%%s%%s' % (n, n)
		return fs % (self.position[0], self.position[1], col1, str(self), col2)

	def attr(self, att, default=None):
		"retrieve an attribute from the instance or from the class (microoptimization here)"
		ret = getattr(self, att, self)
		if ret is self: return getattr(self.__class__, att, default)
		return ret

	def hash_constraints(self):
		"identify a task type for all the constraints relevant for the scheduler: precedence, file production"
		a = self.attr
		sum = hash((self.__class__.__name__,
			str(a('before', '')),
			str(a('after', '')),
			str(a('ext_in', '')),
			str(a('ext_out', '')),
			self.__class__.maxjobs))
		return sum

	def format_error(self):
		"error message to display to the user (when a build fails)"
		if getattr(self, "err_msg", None):
			return self.err_msg
		elif self.hasrun == CRASHED:
			try:
				return " -> task failed (err #%d): %r" % (self.err_code, self)
			except AttributeError:
				return " -> task failed: %r" % self
		elif self.hasrun == MISSING:
			return " -> missing files: %r" % self
		else:
			return ''

	def install(self):
		"""
		installation is performed by looking at the task attributes:
		* install_path: installation path like "${PREFIX}/bin"
		* filename: install the first node in the outputs as a file with a particular name, be certain to give os.sep
		* chmod: permissions
		"""
		bld = self.generator.bld
		d = self.attr('install')

		if self.attr('install_path'):
			lst = [a.relpath_gen(bld.srcnode) for a in self.outputs]
			perm = self.attr('chmod', O644)
			if self.attr('src'):
				# if src is given, install the sources too
				lst += [a.relpath_gen(bld.srcnode) for a in self.inputs]
			if self.attr('filename'):
				dir = self.install_path.rstrip(os.sep) + os.sep + self.attr('filename')
				bld.install_as(dir, lst[0], self.env, perm)
			else:
				bld.install_files(self.install_path, lst, self.env, perm)

class Task(TaskBase):
	"""The parent class is quite limited, in this version:
	* file system interaction: input and output nodes
	* persistence: do not re-execute tasks that have already run
	* caching: same files can be saved and retrieved from a cache directory
	* dependencies:
		implicit, like .c files depending on .h files
		explicit, like the input nodes or the dep_nodes
		environment variables, like the CXXFLAGS in self.env
	"""
	vars = []
	def __init__(self, env, **kw):
		TaskBase.__init__(self, **kw)
		self.env = env

		# inputs and outputs are nodes
		# use setters when possible
		self.inputs  = []
		self.outputs = []

		self.deps_nodes = []
		self.run_after = []

		# Additionally, you may define the following
		#self.dep_vars  = 'PREFIX DATADIR'

	def __str__(self):
		"string to display to the user"
		env = self.env
		src_str = ' '.join([a.nice_path(env) for a in self.inputs])
		tgt_str = ' '.join([a.nice_path(env) for a in self.outputs])
		if self.outputs: sep = ' -> '
		else: sep = ''
		return '%s: %s%s%s\n' % (self.__class__.__name__.replace('_task', ''), src_str, sep, tgt_str)

	def __repr__(self):
		return "".join(['\n\t{task: ', self.__class__.__name__, " ", ",".join([x.name for x in self.inputs]), " -> ", ",".join([x.name for x in self.outputs]), '}'])

	def unique_id(self):
		"get a unique id: hash the node paths, the variant, the class, the function"
		try:
			return self.uid
		except AttributeError:
			"this is not a real hot zone, but we want to avoid surprizes here"
			m = md5()
			up = m.update
			up(self.__class__.__name__)
			up(self.env.variant())
			p = None
			for x in self.inputs + self.outputs:
				if p != x.parent.id:
					p = x.parent.id
					up(x.parent.abspath())
				up(x.name)
			self.uid = m.digest()
			return self.uid

	def set_inputs(self, inp):
		if isinstance(inp, list): self.inputs += inp
		else: self.inputs.append(inp)

	def set_outputs(self, out):
		if isinstance(out, list): self.outputs += out
		else: self.outputs.append(out)

	def set_run_after(self, task):
		"set (scheduler) order on another task"
		# TODO: handle list or object
		assert isinstance(task, TaskBase)
		self.run_after.append(task)

	def add_file_dependency(self, filename):
		"TODO user-provided file dependencies"
		node = self.generator.bld.path.find_resource(filename)
		self.deps_nodes.append(node)

	def signature(self):
		# compute the result one time, and suppose the scan_signature will give the good result
		try: return self.cache_sig[0]
		except AttributeError: pass

		self.m = md5()

		# explicit deps
		exp_sig = self.sig_explicit_deps()

		# env vars
		var_sig = self.sig_vars()

		# implicit deps

		imp_sig = SIG_NIL
		if self.scan:
			try:
				imp_sig = self.sig_implicit_deps()
			except ValueError:
				return self.signature()

		# we now have the signature (first element) and the details (for debugging)
		ret = self.m.digest()
		self.cache_sig = (ret, exp_sig, imp_sig, var_sig)
		return ret

	def runnable_status(self):
		"SKIP_ME RUN_ME or ASK_LATER"
		#return 0 # benchmarking

		if self.inputs and (not self.outputs):
			if not getattr(self.__class__, 'quiet', None):
				warn("invalid task (no inputs OR outputs): override in a Task subclass or set the attribute 'quiet' %r" % self)

		for t in self.run_after:
			if not t.hasrun:
				return ASK_LATER

		env = self.env
		bld = self.generator.bld

		# first compute the signature
		new_sig = self.signature()

		# compare the signature to a signature computed previously
		key = self.unique_id()
		try:
			prev_sig = bld.task_sigs[key][0]
		except KeyError:
			debug("task: task %r must run as it was never run before or the task code changed", self)
			return RUN_ME

		# compare the signatures of the outputs
		for node in self.outputs:
			variant = node.variant(env)
			try:
				if bld.node_sigs[variant][node.id] != new_sig:
					return RUN_ME
			except KeyError:
				debug("task: task %r must run as the output nodes do not exist", self)
				return RUN_ME

		# debug if asked to
		if Logs.verbose: self.debug_why(bld.task_sigs[key])

		if new_sig != prev_sig:
			return RUN_ME
		return SKIP_ME

	def post_run(self):
		"called after a successful task run"
		bld = self.generator.bld
		env = self.env
		sig = self.signature()
		ssig = sig.encode('hex')

		variant = env.variant()
		for node in self.outputs:
			# check if the node exists ..
			try:
				os.stat(node.abspath(env))
			except OSError:
				self.hasrun = MISSING
				self.err_msg = '-> missing file: %r' % node.abspath(env)
				raise Utils.WafError

			# important, store the signature for the next run
			bld.node_sigs[variant][node.id] = sig
		bld.task_sigs[self.unique_id()] = self.cache_sig

		# file caching, if possible
		# try to avoid data corruption as much as possible
		if not Options.cache_global or Options.options.nocache or not self.outputs:
			return None

		if getattr(self, 'cached', None):
			return None

		dname = os.path.join(Options.cache_global, ssig)
		tmpdir = tempfile.mkdtemp(prefix=Options.cache_global)

		try:
			shutil.rmtree(dname)
		except:
			pass

		try:
			for node in self.outputs:
				variant = node.variant(env)
				dest = os.path.join(tmpdir, node.name)
				shutil.copy2(node.abspath(env), dest)
		except (OSError, IOError):
			try:
				shutil.rmtree(tmpdir)
			except:
				pass
		else:
			try:
				os.rename(tmpdir, dname)
			except OSError:
				try:
					shutil.rmtree(tmpdir)
				except:
					pass
			else:
				try:
					os.chmod(dname, O755)
				except:
					pass

	def can_retrieve_cache(self):
		"""
		Retrieve build nodes from the cache
		update the file timestamps to help cleaning the least used entries from the cache
		additionally, set an attribute 'cached' to avoid re-creating the same cache files

		suppose there are files in cache/dir1/file1 and cache/dir2/file2
		first, read the timestamp of dir1
		then try to copy the files
		then look at the timestamp again, if it has changed, the data may have been corrupt (cache update by another process)
		should an exception occur, ignore the data
		"""
		if not Options.cache_global or Options.options.nocache or not self.outputs:
			return None

		env = self.env
		sig = self.signature()
		ssig = sig.encode('hex')

		# first try to access the cache folder for the task
		dname = os.path.join(Options.cache_global, ssig)
		try:
			t1 = os.stat(dname).st_mtime
		except OSError:
			return None

		for node in self.outputs:
			variant = node.variant(env)

			orig = os.path.join(dname, node.name)
			try:
				shutil.copy2(orig, node.abspath(env))
				# mark the cache file as used recently (modified)
				os.utime(orig, None)
			except (OSError, IOError):
				debug('task: failed retrieving file')
				return None

		# is it the same folder?
		try:
			t2 = os.stat(dname).st_mtime
		except OSError:
			return None

		if t1 != t2:
			return None

		for node in self.outputs:
			self.generator.bld.node_sigs[variant][node.id] = sig
			if Options.options.progress_bar < 1:
				self.generator.bld.printout('restoring from cache %r\n' % node.bldpath(env))

		self.cached = True
		return 1

	def debug_why(self, old_sigs):
		"explains why a task is run"

		new_sigs = self.cache_sig
		def v(x):
			return x.encode('hex')

		debug("Task %r", self)
		msgs = ['Task must run', '* Source file or manual dependency', '* Implicit dependency', '* Environment variable']
		tmp = 'task: -> %s: %s %s'
		for x in xrange(len(msgs)):
			if (new_sigs[x] != old_sigs[x]):
				debug(tmp, msgs[x], v(old_sigs[x]), v(new_sigs[x]))

	def sig_explicit_deps(self):
		bld = self.generator.bld
		up = self.m.update

		# the inputs
		for x in self.inputs + getattr(self, 'dep_nodes', []):
			if not x.parent.id in bld.cache_scanned_folders:
				bld.rescan(x.parent)

			variant = x.variant(self.env)
			try:
				up(bld.node_sigs[variant][x.id])
			except KeyError:
				raise Utils.WafError('Missing node signature for %r (required by %r)' % (x, self))

		# manual dependencies, they can slow down the builds
		if bld.deps_man:
			additional_deps = bld.deps_man
			for x in self.inputs + self.outputs:
				try:
					d = additional_deps[x.id]
				except KeyError:
					continue

				for v in d:
					if isinstance(v, Node.Node):
						bld.rescan(v.parent)
						variant = v.variant(self.env)
						try:
							v = bld.node_sigs[variant][v.id]
						except KeyError:
							raise Utils.WafError('Missing node signature for %r (required by %r)' % (v, self))
					elif hasattr(v, '__call__'):
						v = v() # dependency is a function, call it
					up(v)

		for x in self.deps_nodes:
			v = bld.node_sigs[x.variant(self.env)][x.id]
			up(v)

		return self.m.digest()

	def sig_vars(self):
		bld = self.generator.bld
		env = self.env

		# dependencies on the environment vars
		act_sig = bld.hash_env_vars(env, self.__class__.vars)
		self.m.update(act_sig)

		# additional variable dependencies, if provided
		dep_vars = getattr(self, 'dep_vars', None)
		if dep_vars:
			self.m.update(bld.hash_env_vars(env, dep_vars))

		return self.m.digest()

	#def scan(self, node):
	#	"""this method returns a tuple containing:
	#	* a list of nodes corresponding to real files
	#	* a list of names for files not found in path_lst
	#	the input parameters may have more parameters that the ones used below
	#	"""
	#	return ((), ())
	scan = None

	# compute the signature, recompute it if there is no match in the cache
	def sig_implicit_deps(self):
		"the signature obtained may not be the one if the files have changed, we do it in two steps"

		bld = self.generator.bld

		# get the task signatures from previous runs
		key = self.unique_id()
		prev_sigs = bld.task_sigs.get(key, ())
		if prev_sigs:
			try:
				# for issue #379
				if prev_sigs[2] == self.compute_sig_implicit_deps():
					return prev_sigs[2]
			except (KeyError, OSError):
				pass
			del bld.task_sigs[key]
			raise ValueError('rescan')

		# no previous run or the signature of the dependencies has changed, rescan the dependencies
		(nodes, names) = self.scan()
		if Logs.verbose:
			debug('deps: scanner for %s returned %s %s', str(self), str(nodes), str(names))

		# store the dependencies in the cache
		bld.node_deps[key] = nodes
		bld.raw_deps[key] = names

		# recompute the signature and return it
		try:
			sig = self.compute_sig_implicit_deps()
		except KeyError:
			try:
				nodes = []
				for k in bld.node_deps.get(self.unique_id(), []):
					if k.id & 3 == 2: # Node.FILE:
						if not k.id in bld.node_sigs[0]:
							nodes.append(k)
					else:
						if not k.id in bld.node_sigs[self.env.variant()]:
							nodes.append(k)
			except:
				nodes = '?'
			raise Utils.WafError('Missing node signature for %r (for implicit dependencies %r)' % (nodes, self))

		return sig

	def compute_sig_implicit_deps(self):
		"""it is intended for .cpp and inferred .h files
		there is a single list (no tree traversal)
		this is the hot spot so ... do not touch"""
		upd = self.m.update

		bld = self.generator.bld
		tstamp = bld.node_sigs
		env = self.env

		for k in bld.node_deps.get(self.unique_id(), []):
			# unlikely but necessary if it happens
			if not k.parent.id in bld.cache_scanned_folders:
				# if the parent folder is removed, an OSError may be thrown
				bld.rescan(k.parent)

			# if the parent folder is removed, a KeyError will be thrown
			if k.id & 3 == 2: # Node.FILE:
				upd(tstamp[0][k.id])
			else:
				upd(tstamp[env.variant()][k.id])

		return self.m.digest()

def funex(c):
	dc = {}
	exec(c, dc)
	return dc['f']

reg_act = re.compile(r"(?P<backslash>\\)|(?P<dollar>\$\$)|(?P<subst>\$\{(?P<var>\w+)(?P<code>.*?)\})", re.M)
def compile_fun_shell(name, line):
	"""Compiles a string (once) into a function, eg:
	simple_task_type('c++', '${CXX} -o ${TGT[0]} ${SRC} -I ${SRC[0].parent.bldpath()}')

	The env variables (CXX, ..) on the task must not hold dicts (order)
	The reserved keywords TGT and SRC represent the task input and output nodes

	quick test:
	bld(source='wscript', rule='echo "foo\\${SRC[0].name}\\bar"')
	"""

	extr = []
	def repl(match):
		g = match.group
		if g('dollar'): return "$"
		elif g('backslash'): return '\\\\'
		elif g('subst'): extr.append((g('var'), g('code'))); return "%s"
		return None

	line = reg_act.sub(repl, line)

	parm = []
	dvars = []
	app = parm.append
	for (var, meth) in extr:
		if var == 'SRC':
			if meth: app('task.inputs%s' % meth)
			else: app('" ".join([a.srcpath(env) for a in task.inputs])')
		elif var == 'TGT':
			if meth: app('task.outputs%s' % meth)
			else: app('" ".join([a.bldpath(env) for a in task.outputs])')
		else:
			if not var in dvars: dvars.append(var)
			app("p('%s')" % var)
	if parm: parm = "%% (%s) " % (',\n\t\t'.join(parm))
	else: parm = ''

	c = COMPILE_TEMPLATE_SHELL % (line, parm)

	debug('action: %s', c)
	return (funex(c), dvars)

def compile_fun_noshell(name, line):

	extr = []
	def repl(match):
		g = match.group
		if g('dollar'): return "$"
		elif g('subst'): extr.append((g('var'), g('code'))); return "<<|@|>>"
		return None

	line2 = reg_act.sub(repl, line)
	params = line2.split('<<|@|>>')

	buf = []
	dvars = []
	app = buf.append
	for x in xrange(len(extr)):
		params[x] = params[x].strip()
		if params[x]:
			app("lst.extend(%r)" % params[x].split())
		(var, meth) = extr[x]
		if var == 'SRC':
			if meth: app('lst.append(task.inputs%s)' % meth)
			else: app("lst.extend([a.srcpath(env) for a in task.inputs])")
		elif var == 'TGT':
			if meth: app('lst.append(task.outputs%s)' % meth)
			else: app("lst.extend([a.bldpath(env) for a in task.outputs])")
		else:
			app('lst.extend(to_list(env[%r]))' % var)
			if not var in dvars: dvars.append(var)

	if params[-1]:
		app("lst.extend(%r)" % shlex.split(params[-1]))

	fun = COMPILE_TEMPLATE_NOSHELL % "\n\t".join(buf)
	debug('action: %s', fun)
	return (funex(fun), dvars)

def compile_fun(name, line, shell=None):
	"commands can be launched by the shell or not"
	if line.find('<') > 0 or line.find('>') > 0 or line.find('&&') > 0:
		shell = True
	#else:
	#	shell = False

	if shell is None:
		if sys.platform == 'win32':
			shell = False
		else:
			shell = True

	if shell:
		return compile_fun_shell(name, line)
	else:
		return compile_fun_noshell(name, line)

def simple_task_type(name, line, color='GREEN', vars=[], ext_in=[], ext_out=[], before=[], after=[], shell=None):
	"""return a new Task subclass with the function run compiled from the line given"""
	(fun, dvars) = compile_fun(name, line, shell)
	fun.code = line
	return task_type_from_func(name, fun, vars or dvars, color, ext_in, ext_out, before, after)

def task_type_from_func(name, func, vars=[], color='GREEN', ext_in=[], ext_out=[], before=[], after=[]):
	"""return a new Task subclass with the function run compiled from the line given"""
	params = {
		'run': func,
		'vars': vars,
		'color': color,
		'name': name,
		'ext_in': Utils.to_list(ext_in),
		'ext_out': Utils.to_list(ext_out),
		'before': Utils.to_list(before),
		'after': Utils.to_list(after),
	}

	cls = type(Task)(name, (Task,), params)
	TaskBase.classes[name] = cls
	return cls

def always_run(cls):
	"""Set all task instances of this class to be executed whenever a build is started
	The task signature is calculated, but the result of the comparation between
	task signatures is bypassed
	"""
	old = cls.runnable_status
	def always(self):
		old(self)
		return RUN_ME
	cls.runnable_status = always

def update_outputs(cls):
	"""When a command is always run, it is possible that the output only change
	sometimes. By default the build node have as a hash the signature of the task
	which may not change. With this, the output nodes (produced) are hashed,
	and the hashes are set to the build nodes

	This may avoid unnecessary recompilations, but it uses more resources
	(hashing the output files) so it is not used by default
	"""
	old_post_run = cls.post_run
	def post_run(self):
		old_post_run(self)
		bld = self.outputs[0].__class__.bld
		for output in self.outputs:
			bld.node_sigs[self.env.variant()][output.id] = Utils.h_file(output.abspath(self.env))
	cls.post_run = post_run

	old_runnable_status = cls.runnable_status
	def runnable_status(self):
		status = old_runnable_status(self)
		if status != RUN_ME:
			return status

		try:
			bld = self.outputs[0].__class__.bld
			new_sig  = self.signature()
			prev_sig = bld.task_sigs[self.unique_id()][0]
			if prev_sig == new_sig:
				for x in self.outputs:
					if not x.id in bld.node_sigs[self.env.variant()]:
						return RUN_ME
				return SKIP_ME
		except KeyError:
			pass
		except IndexError:
			pass
		return RUN_ME
	cls.runnable_status = runnable_status

def extract_outputs(tasks):
	"""file_deps: Infer additional dependencies from task input and output nodes
	"""
	v = {}
	for x in tasks:
		try:
			(ins, outs) = v[x.env.variant()]
		except KeyError:
			ins = {}
			outs = {}
			v[x.env.variant()] = (ins, outs)

		for a in getattr(x, 'inputs', []):
			try: ins[a.id].append(x)
			except KeyError: ins[a.id] = [x]
		for a in getattr(x, 'outputs', []):
			try: outs[a.id].append(x)
			except KeyError: outs[a.id] = [x]

	for (ins, outs) in v.values():
		links = set(ins.iterkeys()).intersection(outs.iterkeys())
		for k in links:
			for a in ins[k]:
				for b in outs[k]:
					a.set_run_after(b)

def extract_deps(tasks):
	"""file_deps: Infer additional dependencies from task input and output nodes and from implicit dependencies
	returned by the scanners - that will only work if all tasks are created

	this is aimed at people who have pathological builds and who do not care enough
	to implement the build dependencies properly

	with two loops over the list of tasks, do not expect this to be really fast
	"""

	# first reuse the function above
	extract_outputs(tasks)

	# map the output nodes to the tasks producing them
	out_to_task = {}
	for x in tasks:
		v = x.env.variant()
		try:
			lst = x.outputs
		except AttributeError:
			pass
		else:
			for node in lst:
				out_to_task[(v, node.id)] = x

	# map the dependencies found to the tasks compiled
	dep_to_task = {}
	for x in tasks:
		try:
			x.signature()
		except: # this is on purpose
			pass

		v = x.env.variant()
		key = x.unique_id()
		for k in x.generator.bld.node_deps.get(x.unique_id(), []):
			try: dep_to_task[(v, k.id)].append(x)
			except KeyError: dep_to_task[(v, k.id)] = [x]

	# now get the intersection
	deps = set(dep_to_task.keys()).intersection(set(out_to_task.keys()))

	# and add the dependencies from task to task
	for idx in deps:
		for k in dep_to_task[idx]:
			k.set_run_after(out_to_task[idx])

	# cleanup, remove the signatures
	for x in tasks:
		try:
			delattr(x, 'cache_sig')
		except AttributeError:
			pass

