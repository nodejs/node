#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"ocaml support"

import os, re
import TaskGen, Utils, Task, Build
from Logs import error
from TaskGen import taskgen, feature, before, after, extension

EXT_MLL = ['.mll']
EXT_MLY = ['.mly']
EXT_MLI = ['.mli']
EXT_MLC = ['.c']
EXT_ML  = ['.ml']

open_re = re.compile('^\s*open\s+([a-zA-Z]+)(;;){0,1}$', re.M)
foo = re.compile(r"""(\(\*)|(\*\))|("(\\.|[^"\\])*"|'(\\.|[^'\\])*'|.[^()*"'\\]*)""", re.M)
def filter_comments(txt):
	meh = [0]
	def repl(m):
		if m.group(1): meh[0] += 1
		elif m.group(2): meh[0] -= 1
		elif not meh[0]: return m.group(0)
		return ''
	return foo.sub(repl, txt)

def scan(self):
	node = self.inputs[0]
	code = filter_comments(node.read(self.env))

	global open_re
	names = []
	import_iterator = open_re.finditer(code)
	if import_iterator:
		for import_match in import_iterator:
			names.append(import_match.group(1))
	found_lst = []
	raw_lst = []
	for name in names:
		nd = None
		for x in self.incpaths:
			nd = x.find_resource(name.lower()+'.ml')
			if not nd: nd = x.find_resource(name+'.ml')
			if nd:
				found_lst.append(nd)
				break
		else:
			raw_lst.append(name)

	return (found_lst, raw_lst)

native_lst=['native', 'all', 'c_object']
bytecode_lst=['bytecode', 'all']
class ocaml_taskgen(TaskGen.task_gen):
	def __init__(self, *k, **kw):
		TaskGen.task_gen.__init__(self, *k, **kw)

@feature('ocaml')
def init_ml(self):
	Utils.def_attrs(self,
		type = 'all',
		incpaths_lst = [],
		bld_incpaths_lst = [],
		mlltasks = [],
		mlytasks = [],
		mlitasks = [],
		native_tasks = [],
		bytecode_tasks = [],
		linktasks = [],
		bytecode_env = None,
		native_env = None,
		compiled_tasks = [],
		includes = '',
		uselib = '',
		are_deps_set = 0)

@feature('ocaml')
@after('init_ml')
def init_envs_ml(self):

	self.islibrary = getattr(self, 'islibrary', False)

	global native_lst, bytecode_lst
	self.native_env = None
	if self.type in native_lst:
		self.native_env = self.env.copy()
		if self.islibrary: self.native_env['OCALINKFLAGS']   = '-a'

	self.bytecode_env = None
	if self.type in bytecode_lst:
		self.bytecode_env = self.env.copy()
		if self.islibrary: self.bytecode_env['OCALINKFLAGS'] = '-a'

	if self.type == 'c_object':
		self.native_env.append_unique('OCALINKFLAGS_OPT', '-output-obj')

@feature('ocaml')
@before('apply_vars_ml')
@after('init_envs_ml')
def apply_incpaths_ml(self):
	inc_lst = self.includes.split()
	lst = self.incpaths_lst
	for dir in inc_lst:
		node = self.path.find_dir(dir)
		if not node:
			error("node not found: " + str(dir))
			continue
		self.bld.rescan(node)
		if not node in lst: lst.append(node)
		self.bld_incpaths_lst.append(node)
	# now the nodes are added to self.incpaths_lst

@feature('ocaml')
@before('apply_core')
def apply_vars_ml(self):
	for i in self.incpaths_lst:
		if self.bytecode_env:
			app = self.bytecode_env.append_value
			app('OCAMLPATH', '-I')
			app('OCAMLPATH', i.srcpath(self.env))
			app('OCAMLPATH', '-I')
			app('OCAMLPATH', i.bldpath(self.env))

		if self.native_env:
			app = self.native_env.append_value
			app('OCAMLPATH', '-I')
			app('OCAMLPATH', i.bldpath(self.env))
			app('OCAMLPATH', '-I')
			app('OCAMLPATH', i.srcpath(self.env))

	varnames = ['INCLUDES', 'OCAMLFLAGS', 'OCALINKFLAGS', 'OCALINKFLAGS_OPT']
	for name in self.uselib.split():
		for vname in varnames:
			cnt = self.env[vname+'_'+name]
			if cnt:
				if self.bytecode_env: self.bytecode_env.append_value(vname, cnt)
				if self.native_env: self.native_env.append_value(vname, cnt)

@feature('ocaml')
@after('apply_core')
def apply_link_ml(self):

	if self.bytecode_env:
		ext = self.islibrary and '.cma' or '.run'

		linktask = self.create_task('ocalink')
		linktask.bytecode = 1
		linktask.set_outputs(self.path.find_or_declare(self.target + ext))
		linktask.obj = self
		linktask.env = self.bytecode_env
		self.linktasks.append(linktask)

	if self.native_env:
		if self.type == 'c_object': ext = '.o'
		elif self.islibrary: ext = '.cmxa'
		else: ext = ''

		linktask = self.create_task('ocalinkx')
		linktask.set_outputs(self.path.find_or_declare(self.target + ext))
		linktask.obj = self
		linktask.env = self.native_env
		self.linktasks.append(linktask)

		# we produce a .o file to be used by gcc
		self.compiled_tasks.append(linktask)

@extension(EXT_MLL)
def mll_hook(self, node):
	mll_task = self.create_task('ocamllex', node, node.change_ext('.ml'), env=self.native_env)
	self.mlltasks.append(mll_task)

	self.allnodes.append(mll_task.outputs[0])

@extension(EXT_MLY)
def mly_hook(self, node):
	mly_task = self.create_task('ocamlyacc', node, [node.change_ext('.ml'), node.change_ext('.mli')], env=self.native_env)
	self.mlytasks.append(mly_task)
	self.allnodes.append(mly_task.outputs[0])

	task = self.create_task('ocamlcmi', mly_task.outputs[1], mly_task.outputs[1].change_ext('.cmi'), env=self.native_env)

@extension(EXT_MLI)
def mli_hook(self, node):
	task = self.create_task('ocamlcmi', node, node.change_ext('.cmi'), env=self.native_env)
	self.mlitasks.append(task)

@extension(EXT_MLC)
def mlc_hook(self, node):
	task = self.create_task('ocamlcc', node, node.change_ext('.o'), env=self.native_env)
	self.compiled_tasks.append(task)

@extension(EXT_ML)
def ml_hook(self, node):
	if self.native_env:
		task = self.create_task('ocamlx', node, node.change_ext('.cmx'), env=self.native_env)
		task.obj = self
		task.incpaths = self.bld_incpaths_lst
		self.native_tasks.append(task)

	if self.bytecode_env:
		task = self.create_task('ocaml', node, node.change_ext('.cmo'), env=self.bytecode_env)
		task.obj = self
		task.bytecode = 1
		task.incpaths = self.bld_incpaths_lst
		self.bytecode_tasks.append(task)

def compile_may_start(self):
	if not getattr(self, 'flag_deps', ''):
		self.flag_deps = 1

		# the evil part is that we can only compute the dependencies after the
		# source files can be read (this means actually producing the source files)
		if getattr(self, 'bytecode', ''): alltasks = self.obj.bytecode_tasks
		else: alltasks = self.obj.native_tasks

		self.signature() # ensure that files are scanned - unfortunately
		tree = self.generator.bld
		env = self.env
		for node in self.inputs:
			lst = tree.node_deps[self.unique_id()]
			for depnode in lst:
				for t in alltasks:
					if t == self: continue
					if depnode in t.inputs:
						self.set_run_after(t)

		# TODO necessary to get the signature right - for now
		delattr(self, 'cache_sig')
		self.signature()

	return Task.Task.runnable_status(self)

b = Task.simple_task_type
cls = b('ocamlx', '${OCAMLOPT} ${OCAMLPATH} ${OCAMLFLAGS} ${INCLUDES} -c -o ${TGT} ${SRC}', color='GREEN', shell=False)
cls.runnable_status = compile_may_start
cls.scan = scan

b = Task.simple_task_type
cls = b('ocaml', '${OCAMLC} ${OCAMLPATH} ${OCAMLFLAGS} ${INCLUDES} -c -o ${TGT} ${SRC}', color='GREEN', shell=False)
cls.runnable_status = compile_may_start
cls.scan = scan


b('ocamlcmi', '${OCAMLC} ${OCAMLPATH} ${INCLUDES} -o ${TGT} -c ${SRC}', color='BLUE', before="ocaml ocamlcc ocamlx")
b('ocamlcc', 'cd ${TGT[0].bld_dir(env)} && ${OCAMLOPT} ${OCAMLFLAGS} ${OCAMLPATH} ${INCLUDES} -c ${SRC[0].abspath(env)}', color='GREEN')

b('ocamllex', '${OCAMLLEX} ${SRC} -o ${TGT}', color='BLUE', before="ocamlcmi ocaml ocamlcc")
b('ocamlyacc', '${OCAMLYACC} -b ${TGT[0].bld_base(env)} ${SRC}', color='BLUE', before="ocamlcmi ocaml ocamlcc")


def link_may_start(self):
	if not getattr(self, 'order', ''):

		# now reorder the inputs given the task dependencies
		if getattr(self, 'bytecode', 0): alltasks = self.obj.bytecode_tasks
		else: alltasks = self.obj.native_tasks

		# this part is difficult, we do not have a total order on the tasks
		# if the dependencies are wrong, this may not stop
		seen = []
		pendant = []+alltasks
		while pendant:
			task = pendant.pop(0)
			if task in seen: continue
			for x in task.run_after:
				if not x in seen:
					pendant.append(task)
					break
			else:
				seen.append(task)
		self.inputs = [x.outputs[0] for x in seen]
		self.order = 1
	return Task.Task.runnable_status(self)

act = b('ocalink', '${OCAMLC} -o ${TGT} ${INCLUDES} ${OCALINKFLAGS} ${SRC}', color='YELLOW', after="ocaml ocamlcc")
act.runnable_status = link_may_start
act = b('ocalinkx', '${OCAMLOPT} -o ${TGT} ${INCLUDES} ${OCALINKFLAGS_OPT} ${SRC}', color='YELLOW', after="ocamlx ocamlcc")
act.runnable_status = link_may_start

def detect(conf):
	opt = conf.find_program('ocamlopt', var='OCAMLOPT')
	occ = conf.find_program('ocamlc', var='OCAMLC')
	if (not opt) or (not occ):
		conf.fatal('The objective caml compiler was not found:\ninstall it or make it available in your PATH')

	v = conf.env
	v['OCAMLC']       = occ
	v['OCAMLOPT']     = opt
	v['OCAMLLEX']     = conf.find_program('ocamllex', var='OCAMLLEX')
	v['OCAMLYACC']    = conf.find_program('ocamlyacc', var='OCAMLYACC')
	v['OCAMLFLAGS']   = ''
	v['OCAMLLIB']     = Utils.cmd_output(conf.env['OCAMLC']+' -where').strip()+os.sep
	v['LIBPATH_OCAML'] = Utils.cmd_output(conf.env['OCAMLC']+' -where').strip()+os.sep
	v['CPPPATH_OCAML'] = Utils.cmd_output(conf.env['OCAMLC']+' -where').strip()+os.sep
	v['LIB_OCAML'] = 'camlrun'

