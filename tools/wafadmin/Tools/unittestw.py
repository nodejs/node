#!/usr/bin/env python
# encoding: utf-8
# Carlos Rafael Giani, 2006

"""
Unit tests run in the shutdown() method, and for c/c++ programs

One should NOT have to give parameters to programs to execute

In the shutdown method, add the following code:

	>>> def shutdown():
	...	ut = UnitTest.unit_test()
	...	ut.run()
	...	ut.print_results()


Each object to use as a unit test must be a program and must have X{obj.unit_test=1}
"""
import os, sys
import Build, TaskGen, Utils, Options, Logs, Task
from TaskGen import before, after, feature
from Constants import *

class unit_test(object):
	"Unit test representation"
	def __init__(self):
		self.returncode_ok = 0		# Unit test returncode considered OK. All returncodes differing from this one
						# will cause the unit test to be marked as "FAILED".

		# The following variables are filled with data by run().

		# print_results() uses these for printing the unit test summary,
		# but if there is need for direct access to the results,
		# they can be retrieved here, after calling run().

		self.num_tests_ok = 0		# Number of successful unit tests
		self.num_tests_failed = 0	# Number of failed unit tests
		self.num_tests_err = 0		# Tests that have not even run
		self.total_num_tests = 0	# Total amount of unit tests
		self.max_label_length = 0	# Maximum label length (pretty-print the output)

		self.unit_tests = Utils.ordered_dict()		# Unit test dictionary. Key: the label (unit test filename relative
						# to the build dir), value: unit test filename with absolute path
		self.unit_test_results = {}	# Dictionary containing the unit test results.
						# Key: the label, value: result (true = success false = failure)
		self.unit_test_erroneous = {}	# Dictionary indicating erroneous unit tests.
						# Key: the label, value: true = unit test has an error  false = unit test is ok
		self.change_to_testfile_dir = False #True if the test file needs to be executed from the same dir
		self.want_to_see_test_output = False #True to see the stdout from the testfile (for example check suites)
		self.want_to_see_test_error = False #True to see the stderr from the testfile (for example check suites)
		self.run_if_waf_does = 'check' #build was the old default

	def run(self):
		"Run the unit tests and gather results (note: no output here)"

		self.num_tests_ok = 0
		self.num_tests_failed = 0
		self.num_tests_err = 0
		self.total_num_tests = 0
		self.max_label_length = 0

		self.unit_tests = Utils.ordered_dict()
		self.unit_test_results = {}
		self.unit_test_erroneous = {}

		ld_library_path = []

		# If waf is not building, don't run anything
		if not Options.commands[self.run_if_waf_does]: return

		# Get the paths for the shared libraries, and obtain the unit tests to execute
		for obj in Build.bld.all_task_gen:
			try:
				link_task = obj.link_task
			except AttributeError:
				pass
			else:
				lib_path = link_task.outputs[0].parent.abspath(obj.env)
				if lib_path not in ld_library_path:
					ld_library_path.append(lib_path)

			unit_test = getattr(obj, 'unit_test', '')
			if unit_test and 'cprogram' in obj.features:
				try:
					output = obj.path
					filename = os.path.join(output.abspath(obj.env), obj.target)
					srcdir = output.abspath()
					label = os.path.join(output.bldpath(obj.env), obj.target)
					self.max_label_length = max(self.max_label_length, len(label))
					self.unit_tests[label] = (filename, srcdir)
				except KeyError:
					pass
		self.total_num_tests = len(self.unit_tests)
		# Now run the unit tests
		Utils.pprint('GREEN', 'Running the unit tests')
		count = 0
		result = 1

		for label in self.unit_tests.allkeys:
			file_and_src = self.unit_tests[label]
			filename = file_and_src[0]
			srcdir = file_and_src[1]
			count += 1
			line = Build.bld.progress_line(count, self.total_num_tests, Logs.colors.GREEN, Logs.colors.NORMAL)
			if Options.options.progress_bar and line:
				sys.stderr.write(line)
				sys.stderr.flush()
			try:
				kwargs = {}
				kwargs['env'] = os.environ.copy()
				if self.change_to_testfile_dir:
					kwargs['cwd'] = srcdir
				if not self.want_to_see_test_output:
					kwargs['stdout'] = Utils.pproc.PIPE  # PIPE for ignoring output
				if not self.want_to_see_test_error:
					kwargs['stderr'] = Utils.pproc.PIPE  # PIPE for ignoring output
				if ld_library_path:
					v = kwargs['env']
					def add_path(dct, path, var):
						dct[var] = os.pathsep.join(Utils.to_list(path) + [os.environ.get(var, '')])
					if sys.platform == 'win32':
						add_path(v, ld_library_path, 'PATH')
					elif sys.platform == 'darwin':
						add_path(v, ld_library_path, 'DYLD_LIBRARY_PATH')
						add_path(v, ld_library_path, 'LD_LIBRARY_PATH')
					else:
						add_path(v, ld_library_path, 'LD_LIBRARY_PATH')

				pp = Utils.pproc.Popen(filename, **kwargs)
				pp.wait()

				result = int(pp.returncode == self.returncode_ok)

				if result:
					self.num_tests_ok += 1
				else:
					self.num_tests_failed += 1

				self.unit_test_results[label] = result
				self.unit_test_erroneous[label] = 0
			except OSError:
				self.unit_test_erroneous[label] = 1
				self.num_tests_err += 1
			except KeyboardInterrupt:
				pass
		if Options.options.progress_bar: sys.stdout.write(Logs.colors.cursor_on)

	def print_results(self):
		"Pretty-prints a summary of all unit tests, along with some statistics"

		# If waf is not building, don't output anything
		if not Options.commands[self.run_if_waf_does]: return

		p = Utils.pprint
		# Early quit if no tests were performed
		if self.total_num_tests == 0:
			p('YELLOW', 'No unit tests present')
			return

		for label in self.unit_tests.allkeys:
			filename = self.unit_tests[label]
			err = 0
			result = 0

			try: err = self.unit_test_erroneous[label]
			except KeyError: pass

			try: result = self.unit_test_results[label]
			except KeyError: pass

			n = self.max_label_length - len(label)
			if err: n += 4
			elif result: n += 7
			else: n += 3

			line = '%s %s' % (label, '.' * n)

			if err: p('RED', '%sERROR' % line)
			elif result: p('GREEN', '%sOK' % line)
			else: p('YELLOW', '%sFAILED' % line)

		percentage_ok = float(self.num_tests_ok) / float(self.total_num_tests) * 100.0
		percentage_failed = float(self.num_tests_failed) / float(self.total_num_tests) * 100.0
		percentage_erroneous = float(self.num_tests_err) / float(self.total_num_tests) * 100.0

		p('NORMAL', '''
Successful tests:      %i (%.1f%%)
Failed tests:          %i (%.1f%%)
Erroneous tests:       %i (%.1f%%)

Total number of tests: %i
''' % (self.num_tests_ok, percentage_ok, self.num_tests_failed, percentage_failed,
		self.num_tests_err, percentage_erroneous, self.total_num_tests))
		p('GREEN', 'Unit tests finished')


############################################################################################

"""
New unit test system

The targets with feature 'test' are executed after they are built
bld(features='cprogram cc test', ...)

To display the results:
import UnitTest
bld.add_post_fun(UnitTest.summary)
"""

import threading
testlock = threading.Lock()

def set_options(opt):
	opt.add_option('--alltests', action='store_true', default=True, help='Exec all unit tests', dest='all_tests')

@feature('test')
@after('apply_link', 'vars_target_cprogram')
def make_test(self):
	if not 'cprogram' in self.features:
		Logs.error('test cannot be executed %s' % self)
		return

	self.default_install_path = None
	self.create_task('utest', self.link_task.outputs)

def exec_test(self):

	status = 0

	variant = self.env.variant()
	filename = self.inputs[0].abspath(self.env)

	try:
		fu = getattr(self.generator.bld, 'all_test_paths')
	except AttributeError:
		fu = os.environ.copy()
		self.generator.bld.all_test_paths = fu

		lst = []
		for obj in self.generator.bld.all_task_gen:
			link_task = getattr(obj, 'link_task', None)
			if link_task and link_task.env.variant() == variant:
				lst.append(link_task.outputs[0].parent.abspath(obj.env))

		def add_path(dct, path, var):
			dct[var] = os.pathsep.join(Utils.to_list(path) + [os.environ.get(var, '')])

		if sys.platform == 'win32':
			add_path(fu, lst, 'PATH')
		elif sys.platform == 'darwin':
			add_path(fu, lst, 'DYLD_LIBRARY_PATH')
			add_path(fu, lst, 'LD_LIBRARY_PATH')
		else:
			add_path(fu, lst, 'LD_LIBRARY_PATH')


	cwd = getattr(self.generator, 'ut_cwd', '') or self.inputs[0].parent.abspath(self.env)
	proc = Utils.pproc.Popen(filename, cwd=cwd, env=fu, stderr=Utils.pproc.PIPE, stdout=Utils.pproc.PIPE)
	(stdout, stderr) = proc.communicate()

	tup = (filename, proc.returncode, stdout, stderr)
	self.generator.utest_result = tup

	testlock.acquire()
	try:
		bld = self.generator.bld
		Logs.debug("ut: %r", tup)
		try:
			bld.utest_results.append(tup)
		except AttributeError:
			bld.utest_results = [tup]
	finally:
		testlock.release()

cls = Task.task_type_from_func('utest', func=exec_test, color='PINK', ext_in='.bin')

old = cls.runnable_status
def test_status(self):
	if getattr(Options.options, 'all_tests', False):
		return RUN_ME
	return old(self)

cls.runnable_status = test_status
cls.quiet = 1

def summary(bld):
	lst = getattr(bld, 'utest_results', [])
	if lst:
		Utils.pprint('CYAN', 'execution summary')

		total = len(lst)
		tfail = len([x for x in lst if x[1]])

		Utils.pprint('CYAN', '  tests that pass %d/%d' % (total-tfail, total))
		for (f, code, out, err) in lst:
			if not code:
				Utils.pprint('CYAN', '    %s' % f)

		Utils.pprint('CYAN', '  tests that fail %d/%d' % (tfail, total))
		for (f, code, out, err) in lst:
			if code:
				Utils.pprint('CYAN', '    %s' % f)


