#!/usr/bin/env python
# encoding: utf-8
# Scott Newton, 2005 (scottn)
# Thomas Nagy, 2006 (ita)

"Custom command-line options"

import os, sys, imp, types, tempfile, optparse
import Logs, Utils
from Constants import *

cmds = 'distclean configure build install clean uninstall check dist distcheck'.split()

# TODO remove in waf 1.6 the following two
commands = {}
is_install = False

options = {}
arg_line = []
launch_dir = ''
tooldir = ''
lockfile = os.environ.get('WAFLOCK', '.lock-wscript')
try: cache_global = os.path.abspath(os.environ['WAFCACHE'])
except KeyError: cache_global = ''
platform = Utils.unversioned_sys_platform()
conf_file = 'conf-runs-%s-%d.pickle' % (platform, ABI)

remote_repo = ['http://waf.googlecode.com/svn/']
"""remote directory for the plugins"""


# Such a command-line should work:  JOBS=4 PREFIX=/opt/ DESTDIR=/tmp/ahoj/ waf configure
default_prefix = os.environ.get('PREFIX')
if not default_prefix:
	if platform == 'win32': default_prefix = tempfile.gettempdir()
	else: default_prefix = '/usr/local/'

default_jobs = os.environ.get('JOBS', -1)
if default_jobs < 1:
	try:
		if 'SC_NPROCESSORS_ONLN' in os.sysconf_names:
			default_jobs = os.sysconf('SC_NPROCESSORS_ONLN')
		else:
			default_jobs = int(Utils.cmd_output(['sysctl', '-n', 'hw.ncpu']))
	except:
		if os.name == 'java': # platform.system() == 'Java'
			from java.lang import Runtime
			default_jobs = Runtime.getRuntime().availableProcessors()
		else:
			# environment var defined on win32
			default_jobs = int(os.environ.get('NUMBER_OF_PROCESSORS', 1))

default_destdir = os.environ.get('DESTDIR', '')

def get_usage(self):
	cmds_str = []
	module = Utils.g_module
	if module:
		# create the help messages for commands
		tbl = module.__dict__
		keys = list(tbl.keys())
		keys.sort()

		if 'build' in tbl:
			if not module.build.__doc__:
				module.build.__doc__ = 'builds the project'
		if 'configure' in tbl:
			if not module.configure.__doc__:
				module.configure.__doc__ = 'configures the project'

		ban = ['set_options', 'init', 'shutdown']

		optlst = [x for x in keys if not x in ban
			and type(tbl[x]) is type(parse_args_impl)
			and tbl[x].__doc__
			and not x.startswith('_')]

		just = max([len(x) for x in optlst])

		for x in optlst:
			cmds_str.append('  %s: %s' % (x.ljust(just), tbl[x].__doc__))
		ret = '\n'.join(cmds_str)
	else:
		ret = ' '.join(cmds)
	return '''waf [command] [options]

Main commands (example: ./waf build -j4)
%s
''' % ret


setattr(optparse.OptionParser, 'get_usage', get_usage)

def create_parser(module=None):
	Logs.debug('options: create_parser is called')
	parser = optparse.OptionParser(conflict_handler="resolve", version = 'waf %s (%s)' % (WAFVERSION, WAFREVISION))

	parser.formatter.width = Utils.get_term_cols()
	p = parser.add_option

	p('-j', '--jobs',
		type    = 'int',
		default = default_jobs,
		help    = 'amount of parallel jobs (%r)' % default_jobs,
		dest    = 'jobs')

	p('-k', '--keep',
		action  = 'store_true',
		default = False,
		help    = 'keep running happily on independent task groups',
		dest    = 'keep')

	p('-v', '--verbose',
		action  = 'count',
		default = 0,
		help    = 'verbosity level -v -vv or -vvv [default: 0]',
		dest    = 'verbose')

	p('--nocache',
		action  = 'store_true',
		default = False,
		help    = 'ignore the WAFCACHE (if set)',
		dest    = 'nocache')

	p('--zones',
		action  = 'store',
		default = '',
		help    = 'debugging zones (task_gen, deps, tasks, etc)',
		dest    = 'zones')

	p('-p', '--progress',
		action  = 'count',
		default = 0,
		help    = '-p: progress bar; -pp: ide output',
		dest    = 'progress_bar')

	p('--targets',
		action  = 'store',
		default = '',
		help    = 'build given task generators, e.g. "target1,target2"',
		dest    = 'compile_targets')

	gr = optparse.OptionGroup(parser, 'configuration options')
	parser.add_option_group(gr)
	gr.add_option('-b', '--blddir',
		action  = 'store',
		default = '',
		help    = 'build dir for the project (configuration)',
		dest    = 'blddir')
	gr.add_option('-s', '--srcdir',
		action  = 'store',
		default = '',
		help    = 'src dir for the project (configuration)',
		dest    = 'srcdir')
	gr.add_option('--prefix',
		help    = 'installation prefix (configuration) [default: %r]' % default_prefix,
		default = default_prefix,
		dest    = 'prefix')

	gr = optparse.OptionGroup(parser, 'installation options')
	parser.add_option_group(gr)
	gr.add_option('--destdir',
		help    = 'installation root [default: %r]' % default_destdir,
		default = default_destdir,
		dest    = 'destdir')
	gr.add_option('-f', '--force',
		action  = 'store_true',
		default = False,
		help    = 'force file installation',
		dest    = 'force')

	return parser

def parse_args_impl(parser, _args=None):
	global options, commands, arg_line
	(options, args) = parser.parse_args(args=_args)

	arg_line = args
	#arg_line = args[:] # copy

	# By default, 'waf' is equivalent to 'waf build'
	commands = {}
	for var in cmds: commands[var] = 0
	if not args:
		commands['build'] = 1
		args.append('build')

	# Parse the command arguments
	for arg in args:
		commands[arg] = True

	# the check thing depends on the build
	if 'check' in args:
		idx = args.index('check')
		try:
			bidx = args.index('build')
			if bidx > idx:
				raise ValueError('build before check')
		except ValueError, e:
			args.insert(idx, 'build')

	if args[0] != 'init':
		args.insert(0, 'init')

	# TODO -k => -j0
	if options.keep: options.jobs = 1
	if options.jobs < 1: options.jobs = 1

	if 'install' in sys.argv or 'uninstall' in sys.argv:
		# absolute path only if set
		options.destdir = options.destdir and os.path.abspath(os.path.expanduser(options.destdir))

	Logs.verbose = options.verbose
	Logs.init_log()

	if options.zones:
		Logs.zones = options.zones.split(',')
		if not Logs.verbose: Logs.verbose = 1
	elif Logs.verbose > 0:
		Logs.zones = ['runner']
	if Logs.verbose > 2:
		Logs.zones = ['*']

# TODO waf 1.6
# 1. rename the class to OptionsContext
# 2. instead of a class attribute, use a module (static 'parser')
# 3. parse_args_impl was made in times when we did not know about binding new methods to classes

class Handler(Utils.Context):
	"""loads wscript modules in folders for adding options
	This class should be named 'OptionsContext'
	A method named 'recurse' is bound when used by the module Scripting"""

	parser = None
	# make it possible to access the reference, like Build.bld

	def __init__(self, module=None):
		self.parser = create_parser(module)
		self.cwd = os.getcwd()
		Handler.parser = self

	def add_option(self, *k, **kw):
		self.parser.add_option(*k, **kw)

	def add_option_group(self, *k, **kw):
		return self.parser.add_option_group(*k, **kw)

	def get_option_group(self, opt_str):
		return self.parser.get_option_group(opt_str)

	def sub_options(self, *k, **kw):
		if not k: raise Utils.WscriptError('folder expected')
		self.recurse(k[0], name='set_options')

	def tool_options(self, *k, **kw):
		Utils.python_24_guard()

		if not k[0]:
			raise Utils.WscriptError('invalid tool_options call %r %r' % (k, kw))
		tools = Utils.to_list(k[0])

		# TODO waf 1.6 remove the global variable tooldir
		path = Utils.to_list(kw.get('tdir', kw.get('tooldir', tooldir)))

		for tool in tools:
			tool = tool.replace('++', 'xx')
			if tool == 'java': tool = 'javaw'
			if tool.lower() == 'unittest': tool = 'unittestw'
			module = Utils.load_tool(tool, path)
			try:
				fun = module.set_options
			except AttributeError:
				pass
			else:
				fun(kw.get('option_group', self))

	def parse_args(self, args=None):
		parse_args_impl(self.parser, args)

