#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"""
Custom objects:
 - execute a function everytime
 - copy a file somewhere else
"""

import shutil, re, os
import TaskGen, Node, Task, Utils, Build, Constants
from TaskGen import feature, taskgen, after, before
from Logs import debug

def copy_func(tsk):
	"Make a file copy. This might be used to make other kinds of file processing (even calling a compiler is possible)"
	env = tsk.env
	infile = tsk.inputs[0].abspath(env)
	outfile = tsk.outputs[0].abspath(env)
	try:
		shutil.copy2(infile, outfile)
	except (OSError, IOError):
		return 1
	else:
		if tsk.chmod: os.chmod(outfile, tsk.chmod)
		return 0

def action_process_file_func(tsk):
	"Ask the function attached to the task to process it"
	if not tsk.fun: raise Utils.WafError('task must have a function attached to it for copy_func to work!')
	return tsk.fun(tsk)

class cmd_taskgen(TaskGen.task_gen):
	def __init__(self, *k, **kw):
		TaskGen.task_gen.__init__(self, *k, **kw)

@feature('cmd')
def apply_cmd(self):
	"call a command everytime"
	if not self.fun: raise Utils.WafError('cmdobj needs a function!')
	tsk = Task.TaskBase()
	tsk.fun = self.fun
	tsk.env = self.env
	self.tasks.append(tsk)
	tsk.install_path = self.install_path

class copy_taskgen(TaskGen.task_gen):
	"By default, make a file copy, if fun is provided, fun will make the copy (or call a compiler, etc)"
	def __init__(self, *k, **kw):
		TaskGen.task_gen.__init__(self, *k, **kw)

@feature('copy')
@before('apply_core')
def apply_copy(self):
	Utils.def_attrs(self, fun=copy_func)
	self.default_install_path = 0

	lst = self.to_list(self.source)
	self.meths.remove('apply_core')

	for filename in lst:
		node = self.path.find_resource(filename)
		if not node: raise Utils.WafError('cannot find input file %s for processing' % filename)

		target = self.target
		if not target or len(lst)>1: target = node.name

		# TODO the file path may be incorrect
		newnode = self.path.find_or_declare(target)

		tsk = self.create_task('copy', node, newnode)
		tsk.fun = self.fun
		tsk.chmod = self.chmod
		tsk.install_path = self.install_path

		if not tsk.env:
			tsk.debug()
			raise Utils.WafError('task without an environment')

def subst_func(tsk):
	"Substitutes variables in a .in file"

	m4_re = re.compile('@(\w+)@', re.M)

	env = tsk.env
	infile = tsk.inputs[0].abspath(env)
	outfile = tsk.outputs[0].abspath(env)

	code = Utils.readf(infile)

	# replace all % by %% to prevent errors by % signs in the input file while string formatting
	code = code.replace('%', '%%')

	s = m4_re.sub(r'%(\1)s', code)

	di = tsk.dict or {}
	if not di:
		names = m4_re.findall(code)
		for i in names:
			di[i] = env.get_flat(i) or env.get_flat(i.upper())

	file = open(outfile, 'w')
	file.write(s % di)
	file.close()
	if tsk.chmod: os.chmod(outfile, tsk.chmod)

class subst_taskgen(TaskGen.task_gen):
	def __init__(self, *k, **kw):
		TaskGen.task_gen.__init__(self, *k, **kw)

@feature('subst')
@before('apply_core')
def apply_subst(self):
	Utils.def_attrs(self, fun=subst_func)
	self.default_install_path = 0
	lst = self.to_list(self.source)
	self.meths.remove('apply_core')

	self.dict = getattr(self, 'dict', {})

	for filename in lst:
		node = self.path.find_resource(filename)
		if not node: raise Utils.WafError('cannot find input file %s for processing' % filename)

		if self.target:
			newnode = self.path.find_or_declare(self.target)
		else:
			newnode = node.change_ext('')

		try:
			self.dict = self.dict.get_merged_dict()
		except AttributeError:
			pass

		if self.dict and not self.env['DICT_HASH']:
			self.env = self.env.copy()
			keys = list(self.dict.keys())
			keys.sort()
			lst = [self.dict[x] for x in keys]
			self.env['DICT_HASH'] = str(Utils.h_list(lst))

		tsk = self.create_task('copy', node, newnode)
		tsk.fun = self.fun
		tsk.dict = self.dict
		tsk.dep_vars = ['DICT_HASH']
		tsk.install_path = self.install_path
		tsk.chmod = self.chmod

		if not tsk.env:
			tsk.debug()
			raise Utils.WafError('task without an environment')

####################
## command-output ####
####################

class cmd_arg(object):
	"""command-output arguments for representing files or folders"""
	def __init__(self, name, template='%s'):
		self.name = name
		self.template = template
		self.node = None

class input_file(cmd_arg):
	def find_node(self, base_path):
		assert isinstance(base_path, Node.Node)
		self.node = base_path.find_resource(self.name)
		if self.node is None:
			raise Utils.WafError("Input file %s not found in " % (self.name, base_path))

	def get_path(self, env, absolute):
		if absolute:
			return self.template % self.node.abspath(env)
		else:
			return self.template % self.node.srcpath(env)

class output_file(cmd_arg):
	def find_node(self, base_path):
		assert isinstance(base_path, Node.Node)
		self.node = base_path.find_or_declare(self.name)
		if self.node is None:
			raise Utils.WafError("Output file %s not found in " % (self.name, base_path))

	def get_path(self, env, absolute):
		if absolute:
			return self.template % self.node.abspath(env)
		else:
			return self.template % self.node.bldpath(env)

class cmd_dir_arg(cmd_arg):
	def find_node(self, base_path):
		assert isinstance(base_path, Node.Node)
		self.node = base_path.find_dir(self.name)
		if self.node is None:
			raise Utils.WafError("Directory %s not found in " % (self.name, base_path))

class input_dir(cmd_dir_arg):
	def get_path(self, dummy_env, dummy_absolute):
		return self.template % self.node.abspath()

class output_dir(cmd_dir_arg):
	def get_path(self, env, dummy_absolute):
		return self.template % self.node.abspath(env)


class command_output(Task.Task):
	color = "BLUE"
	def __init__(self, env, command, command_node, command_args, stdin, stdout, cwd, os_env, stderr):
		Task.Task.__init__(self, env, normal=1)
		assert isinstance(command, (str, Node.Node))
		self.command = command
		self.command_args = command_args
		self.stdin = stdin
		self.stdout = stdout
		self.cwd = cwd
		self.os_env = os_env
		self.stderr = stderr

		if command_node is not None: self.dep_nodes = [command_node]
		self.dep_vars = [] # additional environment variables to look

	def run(self):
		task = self
		#assert len(task.inputs) > 0

		def input_path(node, template):
			if task.cwd is None:
				return template % node.bldpath(task.env)
			else:
				return template % node.abspath()
		def output_path(node, template):
			fun = node.abspath
			if task.cwd is None: fun = node.bldpath
			return template % fun(task.env)

		if isinstance(task.command, Node.Node):
			argv = [input_path(task.command, '%s')]
		else:
			argv = [task.command]

		for arg in task.command_args:
			if isinstance(arg, str):
				argv.append(arg)
			else:
				assert isinstance(arg, cmd_arg)
				argv.append(arg.get_path(task.env, (task.cwd is not None)))

		if task.stdin:
			stdin = open(input_path(task.stdin, '%s'))
		else:
			stdin = None

		if task.stdout:
			stdout = open(output_path(task.stdout, '%s'), "w")
		else:
			stdout = None

		if task.stderr:
			stderr = open(output_path(task.stderr, '%s'), "w")
		else:
			stderr = None

		if task.cwd is None:
			cwd = ('None (actually %r)' % os.getcwd())
		else:
			cwd = repr(task.cwd)
		debug("command-output: cwd=%s, stdin=%r, stdout=%r, argv=%r" %
			     (cwd, stdin, stdout, argv))

		if task.os_env is None:
			os_env = os.environ
		else:
			os_env = task.os_env
		command = Utils.pproc.Popen(argv, stdin=stdin, stdout=stdout, stderr=stderr, cwd=task.cwd, env=os_env)
		return command.wait()

class cmd_output_taskgen(TaskGen.task_gen):
	def __init__(self, *k, **kw):
		TaskGen.task_gen.__init__(self, *k, **kw)

@feature('command-output')
def init_cmd_output(self):
	Utils.def_attrs(self,
		stdin = None,
		stdout = None,
		stderr = None,
		# the command to execute
		command = None,

		# whether it is an external command; otherwise it is assumed
		# to be an executable binary or script that lives in the
		# source or build tree.
		command_is_external = False,

		# extra parameters (argv) to pass to the command (excluding
		# the command itself)
		argv = [],

		# dependencies to other objects -> this is probably not what you want (ita)
		# values must be 'task_gen' instances (not names!)
		dependencies = [],

		# dependencies on env variable contents
		dep_vars = [],

		# input files that are implicit, i.e. they are not
		# stdin, nor are they mentioned explicitly in argv
		hidden_inputs = [],

		# output files that are implicit, i.e. they are not
		# stdout, nor are they mentioned explicitly in argv
		hidden_outputs = [],

		# change the subprocess to this cwd (must use obj.input_dir() or output_dir() here)
		cwd = None,

		# OS environment variables to pass to the subprocess
		# if None, use the default environment variables unchanged
		os_env = None)

@feature('command-output')
@after('init_cmd_output')
def apply_cmd_output(self):
	if self.command is None:
		raise Utils.WafError("command-output missing command")
	if self.command_is_external:
		cmd = self.command
		cmd_node = None
	else:
		cmd_node = self.path.find_resource(self.command)
		assert cmd_node is not None, ('''Could not find command '%s' in source tree.
Hint: if this is an external command,
use command_is_external=True''') % (self.command,)
		cmd = cmd_node

	if self.cwd is None:
		cwd = None
	else:
		assert isinstance(cwd, CmdDirArg)
		self.cwd.find_node(self.path)

	args = []
	inputs = []
	outputs = []

	for arg in self.argv:
		if isinstance(arg, cmd_arg):
			arg.find_node(self.path)
			if isinstance(arg, input_file):
				inputs.append(arg.node)
			if isinstance(arg, output_file):
				outputs.append(arg.node)

	if self.stdout is None:
		stdout = None
	else:
		assert isinstance(self.stdout, str)
		stdout = self.path.find_or_declare(self.stdout)
		if stdout is None:
			raise Utils.WafError("File %s not found" % (self.stdout,))
		outputs.append(stdout)

	if self.stderr is None:
		stderr = None
	else:
		assert isinstance(self.stderr, str)
		stderr = self.path.find_or_declare(self.stderr)
		if stderr is None:
			raise Utils.WafError("File %s not found" % (self.stderr,))
		outputs.append(stderr)

	if self.stdin is None:
		stdin = None
	else:
		assert isinstance(self.stdin, str)
		stdin = self.path.find_resource(self.stdin)
		if stdin is None:
			raise Utils.WafError("File %s not found" % (self.stdin,))
		inputs.append(stdin)

	for hidden_input in self.to_list(self.hidden_inputs):
		node = self.path.find_resource(hidden_input)
		if node is None:
			raise Utils.WafError("File %s not found in dir %s" % (hidden_input, self.path))
		inputs.append(node)

	for hidden_output in self.to_list(self.hidden_outputs):
		node = self.path.find_or_declare(hidden_output)
		if node is None:
			raise Utils.WafError("File %s not found in dir %s" % (hidden_output, self.path))
		outputs.append(node)

	if not (inputs or getattr(self, 'no_inputs', None)):
		raise Utils.WafError('command-output objects must have at least one input file or give self.no_inputs')
	if not (outputs or getattr(self, 'no_outputs', None)):
		raise Utils.WafError('command-output objects must have at least one output file or give self.no_outputs')

	task = command_output(self.env, cmd, cmd_node, self.argv, stdin, stdout, cwd, self.os_env, stderr)
	Utils.copy_attrs(self, task, 'before after ext_in ext_out', only_if_set=True)
	self.tasks.append(task)

	task.inputs = inputs
	task.outputs = outputs
	task.dep_vars = self.to_list(self.dep_vars)

	for dep in self.dependencies:
		assert dep is not self
		dep.post()
		for dep_task in dep.tasks:
			task.set_run_after(dep_task)

	if not task.inputs:
		# the case for svnversion, always run, and update the output nodes
		task.runnable_status = type(Task.TaskBase.run)(runnable_status, task, task.__class__) # always run
		task.post_run = type(Task.TaskBase.run)(post_run, task, task.__class__)

	# TODO the case with no outputs?

def post_run(self):
	for x in self.outputs:
		h = Utils.h_file(x.abspath(self.env))
		self.generator.bld.node_sigs[self.env.variant()][x.id] = h

def runnable_status(self):
	return Constants.RUN_ME

Task.task_type_from_func('copy', vars=[], func=action_process_file_func)
TaskGen.task_gen.classes['command-output'] = cmd_output_taskgen

