#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"TeX/LaTeX/PDFLaTeX support"

import os, re
import Utils, TaskGen, Task, Runner, Build
from TaskGen import feature, before
from Logs import error, warn, debug

re_tex = re.compile(r'\\(?P<type>include|input|import|bringin){(?P<file>[^{}]*)}', re.M)
def scan(self):
	node = self.inputs[0]
	env = self.env

	nodes = []
	names = []
	if not node: return (nodes, names)

	code = Utils.readf(node.abspath(env))

	curdirnode = self.curdirnode
	abs = curdirnode.abspath()
	for match in re_tex.finditer(code):
		path = match.group('file')
		if path:
			for k in ['', '.tex', '.ltx']:
				# add another loop for the tex include paths?
				debug('tex: trying %s%s' % (path, k))
				try:
					os.stat(abs+os.sep+path+k)
				except OSError:
					continue
				found = path+k
				node = curdirnode.find_resource(found)
				if node:
					nodes.append(node)
			else:
				debug('tex: could not find %s' % path)
				names.append(path)

	debug("tex: found the following : %s and names %s" % (nodes, names))
	return (nodes, names)

g_bibtex_re = re.compile('bibdata', re.M)
def tex_build(task, command='LATEX'):
	env = task.env
	bld = task.generator.bld

	com = '%s %s' % (env[command], env.get_flat(command+'FLAGS'))
	if not env['PROMPT_LATEX']: com = "%s %s" % (com, '-interaction=batchmode')

	node = task.inputs[0]
	reldir  = node.bld_dir(env)
	srcfile = node.srcpath(env)

	lst = []
	for c in Utils.split_path(reldir):
		if c: lst.append('..')
	sr = os.path.join(*(lst + [srcfile]))
	sr2 = os.path.join(*(lst + [node.parent.srcpath(env)]))

	aux_node = node.change_ext('.aux')
	idx_node = node.change_ext('.idx')

	hash     = ''
	old_hash = ''

	nm = aux_node.name
	docuname = nm[ : len(nm) - 4 ] # 4 is the size of ".aux"

	latex_compile_cmd = 'cd %s && TEXINPUTS=%s:$TEXINPUTS %s %s' % (reldir, sr2, com, sr)
	warn('first pass on %s' % command)
	ret = bld.exec_command(latex_compile_cmd)
	if ret: return ret

	# look in the .aux file if there is a bibfile to process
	try:
		ct = Utils.readf(aux_node.abspath(env))
	except (OSError, IOError):
		error('error bibtex scan')
	else:
		fo = g_bibtex_re.findall(ct)

		# yes, there is a .aux file to process
		if fo:
			bibtex_compile_cmd = 'cd %s && BIBINPUTS=%s:$BIBINPUTS %s %s' % (reldir, sr2, env['BIBTEX'], docuname)

			warn('calling bibtex')
			ret = bld.exec_command(bibtex_compile_cmd)
			if ret:
				error('error when calling bibtex %s' % bibtex_compile_cmd)
				return ret

	# look on the filesystem if there is a .idx file to process
	try:
		idx_path = idx_node.abspath(env)
		os.stat(idx_path)
	except OSError:
		error('error file.idx scan')
	else:
		makeindex_compile_cmd = 'cd %s && %s %s' % (reldir, env['MAKEINDEX'], idx_path)
		warn('calling makeindex')
		ret = bld.exec_command(makeindex_compile_cmd)
		if ret:
			error('error when calling makeindex %s' % makeindex_compile_cmd)
			return ret

	i = 0
	while i < 10:
		# prevent against infinite loops - one never knows
		i += 1

		# watch the contents of file.aux
		old_hash = hash
		try:
			hash = Utils.h_file(aux_node.abspath(env))
		except KeyError:
			error('could not read aux.h -> %s' % aux_node.abspath(env))
			pass

		# debug
		#print "hash is, ", hash, " ", old_hash

		# stop if file.aux does not change anymore
		if hash and hash == old_hash: break

		# run the command
		warn('calling %s' % command)
		ret = bld.exec_command(latex_compile_cmd)
		if ret:
			error('error when calling %s %s' % (command, latex_compile_cmd))
			return ret

	# 0 means no error
	return 0

latex_vardeps  = ['LATEX', 'LATEXFLAGS']
def latex_build(task):
	return tex_build(task, 'LATEX')

pdflatex_vardeps  = ['PDFLATEX', 'PDFLATEXFLAGS']
def pdflatex_build(task):
	return tex_build(task, 'PDFLATEX')

class tex_taskgen(TaskGen.task_gen):
	def __init__(self, *k, **kw):
		TaskGen.task_gen.__init__(self, *k, **kw)

@feature('tex')
@before('apply_core')
def apply_tex(self):
	if not getattr(self, 'type', None) in ['latex', 'pdflatex']:
		self.type = 'pdflatex'

	tree = self.bld
	outs = Utils.to_list(getattr(self, 'outs', []))

	# prompt for incomplete files (else the batchmode is used)
	self.env['PROMPT_LATEX'] = getattr(self, 'prompt', 1)

	deps_lst = []

	if getattr(self, 'deps', None):
		deps = self.to_list(self.deps)
		for filename in deps:
			n = self.path.find_resource(filename)
			if not n in deps_lst: deps_lst.append(n)

	self.source = self.to_list(self.source)
	for filename in self.source:
		base, ext = os.path.splitext(filename)

		node = self.path.find_resource(filename)
		if not node: raise Utils.WafError('cannot find %s' % filename)

		if self.type == 'latex':
			task = self.create_task('latex')
			task.set_inputs(node)
			task.set_outputs(node.change_ext('.dvi'))
		elif self.type == 'pdflatex':
			task = self.create_task('pdflatex')
			task.set_inputs(node)
			task.set_outputs(node.change_ext('.pdf'))

		task.env = self.env
		task.curdirnode = self.path

		# add the manual dependencies
		if deps_lst:
			variant = node.variant(self.env)
			try:
				lst = tree.node_deps[task.unique_id()]
				for n in deps_lst:
					if not n in lst:
						lst.append(n)
			except KeyError:
				tree.node_deps[task.unique_id()] = deps_lst

		if self.type == 'latex':
			if 'ps' in outs:
				pstask = self.create_task('dvips')
				pstask.set_inputs(task.outputs)
				pstask.set_outputs(node.change_ext('.ps'))
			if 'pdf' in outs:
				pdftask = self.create_task('dvipdf')
				pdftask.set_inputs(task.outputs)
				pdftask.set_outputs(node.change_ext('.pdf'))
		elif self.type == 'pdflatex':
			if 'ps' in outs:
				pstask = self.create_task('pdf2ps')
				pstask.set_inputs(task.outputs)
				pstask.set_outputs(node.change_ext('.ps'))
	self.source = []

def detect(conf):
	v = conf.env
	for p in 'tex latex pdflatex bibtex dvips dvipdf ps2pdf makeindex pdf2ps'.split():
		conf.find_program(p, var=p.upper())
		v[p.upper()+'FLAGS'] = ''
	v['DVIPSFLAGS'] = '-Ppdf'

b = Task.simple_task_type
b('tex', '${TEX} ${TEXFLAGS} ${SRC}', color='BLUE', shell=False)
b('bibtex', '${BIBTEX} ${BIBTEXFLAGS} ${SRC}', color='BLUE', shell=False)
b('dvips', '${DVIPS} ${DVIPSFLAGS} ${SRC} -o ${TGT}', color='BLUE', after="latex pdflatex tex bibtex", shell=False)
b('dvipdf', '${DVIPDF} ${DVIPDFFLAGS} ${SRC} ${TGT}', color='BLUE', after="latex pdflatex tex bibtex", shell=False)
b('pdf2ps', '${PDF2PS} ${PDF2PSFLAGS} ${SRC} ${TGT}', color='BLUE', after="dvipdf pdflatex", shell=False)
b = Task.task_type_from_func
cls = b('latex', latex_build, vars=latex_vardeps)
cls.scan = scan
cls = b('pdflatex', pdflatex_build, vars=pdflatex_vardeps)
cls.scan = scan

