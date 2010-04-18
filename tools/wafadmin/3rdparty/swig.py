#! /usr/bin/env python
# encoding: UTF-8
# Petar Forai
# Thomas Nagy 2008

import re
import Task, Utils, Logs
from TaskGen import extension, taskgen, feature, after
from Configure import conf
import preproc

"""
Welcome in the hell of adding tasks dynamically

swig interface files may be created at runtime, the module name may be unknown in advance

rev 5859 is much more simple
"""

SWIG_EXTS = ['.swig', '.i']

swig_str = '${SWIG} ${SWIGFLAGS} ${SRC}'
cls = Task.simple_task_type('swig', swig_str, color='BLUE', ext_in='.i .h', ext_out='.o .c .cxx', shell=False)

def runnable_status(self):
	for t in self.run_after:
		if not t.hasrun:
			return ASK_LATER

	if not getattr(self, 'init_outputs', None):
		self.init_outputs = True
		if not getattr(self, 'module', None):
			# search the module name
			txt = self.inputs[0].read(self.env)
			m = re_module.search(txt)
			if not m:
				raise ValueError("could not find the swig module name")
			self.module = m.group(1)

		swig_c(self)

		# add the language-specific output files as nodes
		# call funs in the dict swig_langs
		for x in self.env['SWIGFLAGS']:
			# obtain the language
			x = x[1:]
			try:
				fun = swig_langs[x]
			except KeyError:
				pass
			else:
				fun(self)

	return Task.Task.runnable_status(self)
setattr(cls, 'runnable_status', runnable_status)

re_module = re.compile('%module(?:\s*\(.*\))?\s+(.+)', re.M)

re_1 = re.compile(r'^%module.*?\s+([\w]+)\s*?$', re.M)
re_2 = re.compile('%include "(.*)"', re.M)
re_3 = re.compile('#include "(.*)"', re.M)

def scan(self):
	"scan for swig dependencies, climb the .i files"
	env = self.env

	lst_src = []

	seen = []
	to_see = [self.inputs[0]]

	while to_see:
		node = to_see.pop(0)
		if node.id in seen:
			continue
		seen.append(node.id)
		lst_src.append(node)

		# read the file
		code = node.read(env)
		code = preproc.re_nl.sub('', code)
		code = preproc.re_cpp.sub(preproc.repl, code)

		# find .i files and project headers
		names = re_2.findall(code) + re_3.findall(code)
		for n in names:
			for d in self.generator.swig_dir_nodes + [node.parent]:
				u = d.find_resource(n)
				if u:
					to_see.append(u)
					break
			else:
				Logs.warn('could not find %r' % n)

	# list of nodes this one depends on, and module name if present
	if Logs.verbose:
		Logs.debug('deps: deps for %s: %s' % (str(self), str(lst_src)))
	return (lst_src, [])
cls.scan = scan

# provide additional language processing
swig_langs = {}
def swig(fun):
	swig_langs[fun.__name__.replace('swig_', '')] = fun

def swig_c(self):
	ext = '.swigwrap_%d.c' % self.generator.idx
	flags = self.env['SWIGFLAGS']
	if '-c++' in flags:
		ext += 'xx'
	out_node = self.inputs[0].parent.find_or_declare(self.module + ext)

	if '-c++' in flags:
		task = self.generator.cxx_hook(out_node)
	else:
		task = self.generator.cc_hook(out_node)

	task.set_run_after(self)

	ge = self.generator.bld.generator
	ge.outstanding.insert(0, task)
	ge.total += 1

	try:
		ltask = self.generator.link_task
	except AttributeError:
		pass
	else:
		ltask.inputs.append(task.outputs[0])

	self.outputs.append(out_node)

	if not '-o' in self.env['SWIGFLAGS']:
		self.env.append_value('SWIGFLAGS', '-o')
		self.env.append_value('SWIGFLAGS', self.outputs[0].abspath(self.env))

@swig
def swig_python(tsk):
	tsk.set_outputs(tsk.inputs[0].parent.find_or_declare(tsk.module + '.py'))

@swig
def swig_ocaml(tsk):
	tsk.set_outputs(tsk.inputs[0].parent.find_or_declare(tsk.module + '.ml'))
	tsk.set_outputs(tsk.inputs[0].parent.find_or_declare(tsk.module + '.mli'))

@taskgen
@feature('swig')
@after('apply_incpaths')
def add_swig_paths(self):
	"""the attribute 'after' is not used here, the method is added directly at the end"""

	self.swig_dir_nodes = self.env['INC_PATHS']
	include_flags = self.env['_CXXINCFLAGS'] or self.env['_CCINCFLAGS']
	self.env.append_unique('SWIGFLAGS', [f.replace("/I", "-I") for f in include_flags])

@extension(SWIG_EXTS)
def i_file(self, node):
	if not 'add_swig_paths' in self.meths:
		self.meths.append('add_swig_paths')

	# the task instance
	tsk = self.create_task('swig')
	tsk.set_inputs(node)
	tsk.module = getattr(self, 'swig_module', None)

	flags = self.to_list(getattr(self, 'swig_flags', []))
	tsk.env['SWIGFLAGS'] = flags

	if not '-outdir' in flags:
		flags.append('-outdir')
		flags.append(node.parent.abspath(self.env))

@conf
def check_swig_version(conf, minver=None):
	"""Check for a minimum swig version like conf.check_swig_version('1.3.28')
	or conf.check_swig_version((1,3,28)) """
	reg_swig = re.compile(r'SWIG Version\s(.*)', re.M)

	swig_out = Utils.cmd_output('%s -version' % conf.env['SWIG'])

	swigver = [int(s) for s in reg_swig.findall(swig_out)[0].split('.')]
	if isinstance(minver, basestring):
		minver = [int(s) for s in minver.split(".")]
	if isinstance(minver, tuple):
		minver = [int(s) for s in minver]
	result = (minver is None) or (minver[:3] <= swigver[:3])
	swigver_full = '.'.join(map(str, swigver))
	if result:
		conf.env['SWIG_VERSION'] = swigver_full
	minver_str = '.'.join(map(str, minver))
	if minver is None:
		conf.check_message_custom('swig version', '', swigver_full)
	else:
		conf.check_message('swig version', '>= %s' % (minver_str,), result, option=swigver_full)
	return result

def detect(conf):
	swig = conf.find_program('swig', var='SWIG', mandatory=True)

