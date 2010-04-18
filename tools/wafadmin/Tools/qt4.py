#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"""
Qt4 support

If QT4_ROOT is given (absolute path), the configuration will look in it first

This module also demonstrates how to add tasks dynamically (when the build has started)
"""

try:
	from xml.sax import make_parser
	from xml.sax.handler import ContentHandler
except ImportError:
	has_xml = False
	ContentHandler = object
else:
	has_xml = True

import os, sys
import ccroot, cxx
import TaskGen, Task, Utils, Runner, Options, Node, Configure
from TaskGen import taskgen, feature, after, extension
from Logs import error
from Constants import *

MOC_H = ['.h', '.hpp', '.hxx', '.hh']
EXT_RCC = ['.qrc']
EXT_UI  = ['.ui']
EXT_QT4 = ['.cpp', '.cc', '.cxx', '.C']

class qxx_task(Task.Task):
	"A cpp task that may create a moc task dynamically"

	before = ['cxx_link', 'static_link']

	def __init__(self, *k, **kw):
		Task.Task.__init__(self, *k, **kw)
		self.moc_done = 0

	def scan(self):
		(nodes, names) = ccroot.scan(self)
		# for some reasons (variants) the moc node may end in the list of node deps
		for x in nodes:
			if x.name.endswith('.moc'):
				nodes.remove(x)
				names.append(x.relpath_gen(self.inputs[0].parent))
		return (nodes, names)

	def runnable_status(self):
		if self.moc_done:
			# if there is a moc task, delay the computation of the file signature
			for t in self.run_after:
				if not t.hasrun:
					return ASK_LATER
			# the moc file enters in the dependency calculation
			# so we need to recompute the signature when the moc file is present
			self.signature()
			return Task.Task.runnable_status(self)
		else:
			# yes, really, there are people who generate cxx files
			for t in self.run_after:
				if not t.hasrun:
					return ASK_LATER
			self.add_moc_tasks()
			return ASK_LATER

	def add_moc_tasks(self):

		node = self.inputs[0]
		tree = node.__class__.bld

		try:
			# compute the signature once to know if there is a moc file to create
			self.signature()
		except KeyError:
			# the moc file may be referenced somewhere else
			pass
		else:
			# remove the signature, it must be recomputed with the moc task
			delattr(self, 'cache_sig')

		moctasks=[]
		mocfiles=[]
		variant = node.variant(self.env)
		try:
			tmp_lst = tree.raw_deps[self.unique_id()]
			tree.raw_deps[self.unique_id()] = []
		except KeyError:
			tmp_lst = []
		for d in tmp_lst:
			if not d.endswith('.moc'): continue
			# paranoid check
			if d in mocfiles:
				error("paranoia owns")
				continue

			# process that base.moc only once
			mocfiles.append(d)

			# find the extension (performed only when the .cpp has changes)
			base2 = d[:-4]
			for path in [node.parent] + self.generator.env['INC_PATHS']:
				tree.rescan(path)
				vals = getattr(Options.options, 'qt_header_ext', '') or MOC_H
				for ex in vals:
					h_node = path.find_resource(base2 + ex)
					if h_node:
						break
				else:
					continue
				break
			else:
				raise Utils.WafError("no header found for %s which is a moc file" % str(d))

			m_node = h_node.change_ext('.moc')
			tree.node_deps[(self.inputs[0].parent.id, self.env.variant(), m_node.name)] = h_node

			# create the task
			task = Task.TaskBase.classes['moc'](self.env, normal=0)
			task.set_inputs(h_node)
			task.set_outputs(m_node)

			generator = tree.generator
			generator.outstanding.insert(0, task)
			generator.total += 1

			moctasks.append(task)

		# remove raw deps except the moc files to save space (optimization)
		tmp_lst = tree.raw_deps[self.unique_id()] = mocfiles

		# look at the file inputs, it is set right above
		lst = tree.node_deps.get(self.unique_id(), ())
		for d in lst:
			name = d.name
			if name.endswith('.moc'):
				task = Task.TaskBase.classes['moc'](self.env, normal=0)
				task.set_inputs(tree.node_deps[(self.inputs[0].parent.id, self.env.variant(), name)]) # 1st element in a tuple
				task.set_outputs(d)

				generator = tree.generator
				generator.outstanding.insert(0, task)
				generator.total += 1

				moctasks.append(task)

		# simple scheduler dependency: run the moc task before others
		self.run_after = moctasks
		self.moc_done = 1

	run = Task.TaskBase.classes['cxx'].__dict__['run']

def translation_update(task):
	outs = [a.abspath(task.env) for a in task.outputs]
	outs = " ".join(outs)
	lupdate = task.env['QT_LUPDATE']

	for x in task.inputs:
		file = x.abspath(task.env)
		cmd = "%s %s -ts %s" % (lupdate, file, outs)
		Utils.pprint('BLUE', cmd)
		task.generator.bld.exec_command(cmd)

class XMLHandler(ContentHandler):
	def __init__(self):
		self.buf = []
		self.files = []
	def startElement(self, name, attrs):
		if name == 'file':
			self.buf = []
	def endElement(self, name):
		if name == 'file':
			self.files.append(''.join(self.buf))
	def characters(self, cars):
		self.buf.append(cars)

def scan(self):
	"add the dependency on the files referenced in the qrc"
	node = self.inputs[0]
	parser = make_parser()
	curHandler = XMLHandler()
	parser.setContentHandler(curHandler)
	fi = open(self.inputs[0].abspath(self.env))
	parser.parse(fi)
	fi.close()

	nodes = []
	names = []
	root = self.inputs[0].parent
	for x in curHandler.files:
		nd = root.find_resource(x)
		if nd: nodes.append(nd)
		else: names.append(x)

	return (nodes, names)

@extension(EXT_RCC)
def create_rcc_task(self, node):
	"hook for rcc files"
	rcnode = node.change_ext('_rc.cpp')
	rcctask = self.create_task('rcc', node, rcnode)
	cpptask = self.create_task('cxx', rcnode, rcnode.change_ext('.o'))
	self.compiled_tasks.append(cpptask)
	return cpptask

@extension(EXT_UI)
def create_uic_task(self, node):
	"hook for uic tasks"
	uictask = self.create_task('ui4', node)
	uictask.outputs = [self.path.find_or_declare(self.env['ui_PATTERN'] % node.name[:-3])]
	return uictask

class qt4_taskgen(cxx.cxx_taskgen):
	def __init__(self, *k, **kw):
		cxx.cxx_taskgen.__init__(self, *k, **kw)
		self.features.append('qt4')

@extension('.ts')
def add_lang(self, node):
	"""add all the .ts file into self.lang"""
	self.lang = self.to_list(getattr(self, 'lang', [])) + [node]

@feature('qt4')
@after('apply_link')
def apply_qt4(self):
	if getattr(self, 'lang', None):
		update = getattr(self, 'update', None)
		lst=[]
		trans=[]
		for l in self.to_list(self.lang):

			if not isinstance(l, Node.Node):
				l = self.path.find_resource(l+'.ts')

			t = self.create_task('ts2qm', l, l.change_ext('.qm'))
			lst.append(t.outputs[0])

			if update:
				trans.append(t.inputs[0])

		trans_qt4 = getattr(Options.options, 'trans_qt4', False)
		if update and trans_qt4:
			# we need the cpp files given, except the rcc task we create after
			# FIXME may be broken
			u = Task.TaskCmd(translation_update, self.env, 2)
			u.inputs = [a.inputs[0] for a in self.compiled_tasks]
			u.outputs = trans

		if getattr(self, 'langname', None):
			t = Task.TaskBase.classes['qm2rcc'](self.env)
			t.set_inputs(lst)
			t.set_outputs(self.path.find_or_declare(self.langname+'.qrc'))
			t.path = self.path
			k = create_rcc_task(self, t.outputs[0])
			self.link_task.inputs.append(k.outputs[0])

	self.env.append_value('MOC_FLAGS', self.env._CXXDEFFLAGS)
	self.env.append_value('MOC_FLAGS', self.env._CXXINCFLAGS)

@extension(EXT_QT4)
def cxx_hook(self, node):
	# create the compilation task: cpp or cc
	try: obj_ext = self.obj_ext
	except AttributeError: obj_ext = '_%d.o' % self.idx

	task = self.create_task('qxx', node, node.change_ext(obj_ext))
	self.compiled_tasks.append(task)
	return task

def process_qm2rcc(task):
	outfile = task.outputs[0].abspath(task.env)
	f = open(outfile, 'w')
	f.write('<!DOCTYPE RCC><RCC version="1.0">\n<qresource>\n')
	for k in task.inputs:
		f.write(' <file>')
		#f.write(k.name)
		f.write(k.path_to_parent(task.path))
		f.write('</file>\n')
	f.write('</qresource>\n</RCC>')
	f.close()

b = Task.simple_task_type
b('moc', '${QT_MOC} ${MOC_FLAGS} ${SRC} ${MOC_ST} ${TGT}', color='BLUE', vars=['QT_MOC', 'MOC_FLAGS'], shell=False)
cls = b('rcc', '${QT_RCC} -name ${SRC[0].name} ${SRC[0].abspath(env)} ${RCC_ST} -o ${TGT}', color='BLUE', before='cxx moc qxx_task', after="qm2rcc", shell=False)
cls.scan = scan
b('ui4', '${QT_UIC} ${SRC} -o ${TGT}', color='BLUE', before='cxx moc qxx_task', shell=False)
b('ts2qm', '${QT_LRELEASE} ${QT_LRELEASE_FLAGS} ${SRC} -qm ${TGT}', color='BLUE', before='qm2rcc', shell=False)

Task.task_type_from_func('qm2rcc', vars=[], func=process_qm2rcc, color='BLUE', before='rcc', after='ts2qm')

def detect_qt4(conf):
	env = conf.env
	opt = Options.options

	qtdir = getattr(opt, 'qtdir', '')
	qtbin = getattr(opt, 'qtbin', '')
	qtlibs = getattr(opt, 'qtlibs', '')
	useframework = getattr(opt, 'use_qt4_osxframework', True)

	paths = []

	# the path to qmake has been given explicitely
	if qtbin:
		paths = [qtbin]

	# the qt directory has been given - we deduce the qt binary path
	if not qtdir:
		qtdir = conf.environ.get('QT4_ROOT', '')
		qtbin = os.path.join(qtdir, 'bin')
		paths = [qtbin]

	# no qtdir, look in the path and in /usr/local/Trolltech
	if not qtdir:
		paths = os.environ.get('PATH', '').split(os.pathsep)
		paths.append('/usr/share/qt4/bin/')
		try:
			lst = os.listdir('/usr/local/Trolltech/')
		except OSError:
			pass
		else:
			if lst:
				lst.sort()
				lst.reverse()

				# keep the highest version
				qtdir = '/usr/local/Trolltech/%s/' % lst[0]
				qtbin = os.path.join(qtdir, 'bin')
				paths.append(qtbin)

	# at the end, try to find qmake in the paths given
	# keep the one with the highest version
	cand = None
	prev_ver = ['4', '0', '0']
	for qmk in ['qmake-qt4', 'qmake4', 'qmake']:
		qmake = conf.find_program(qmk, path_list=paths)
		if qmake:
			try:
				version = Utils.cmd_output([qmake, '-query', 'QT_VERSION']).strip()
			except ValueError:
				pass
			else:
				if version:
					new_ver = version.split('.')
					if new_ver > prev_ver:
						cand = qmake
						prev_ver = new_ver
	if cand:
		qmake = cand
	else:
		conf.fatal('could not find qmake for qt4')

	conf.env.QMAKE = qmake
	qtincludes = Utils.cmd_output([qmake, '-query', 'QT_INSTALL_HEADERS']).strip()
	qtdir = Utils.cmd_output([qmake, '-query', 'QT_INSTALL_PREFIX']).strip() + os.sep
	qtbin = Utils.cmd_output([qmake, '-query', 'QT_INSTALL_BINS']).strip() + os.sep

	if not qtlibs:
		try:
			qtlibs = Utils.cmd_output([qmake, '-query', 'QT_INSTALL_LIBS']).strip() + os.sep
		except ValueError:
			qtlibs = os.path.join(qtdir, 'lib')

	def find_bin(lst, var):
		for f in lst:
			ret = conf.find_program(f, path_list=paths)
			if ret:
				env[var]=ret
				break

	vars = "QtCore QtGui QtUiTools QtNetwork QtOpenGL QtSql QtSvg QtTest QtXml QtWebKit Qt3Support".split()

	find_bin(['uic-qt3', 'uic3'], 'QT_UIC3')
	find_bin(['uic-qt4', 'uic'], 'QT_UIC')
	if not env['QT_UIC']:
		conf.fatal('cannot find the uic compiler for qt4')

	try:
		version = Utils.cmd_output(env['QT_UIC'] + " -version 2>&1").strip()
	except ValueError:
		conf.fatal('your uic compiler is for qt3, add uic for qt4 to your path')

	version = version.replace('Qt User Interface Compiler ','')
	version = version.replace('User Interface Compiler for Qt', '')
	if version.find(" 3.") != -1:
		conf.check_message('uic version', '(too old)', 0, option='(%s)'%version)
		sys.exit(1)
	conf.check_message('uic version', '', 1, option='(%s)'%version)

	find_bin(['moc-qt4', 'moc'], 'QT_MOC')
	find_bin(['rcc'], 'QT_RCC')
	find_bin(['lrelease-qt4', 'lrelease'], 'QT_LRELEASE')
	find_bin(['lupdate-qt4', 'lupdate'], 'QT_LUPDATE')

	env['UIC3_ST']= '%s -o %s'
	env['UIC_ST'] = '%s -o %s'
	env['MOC_ST'] = '-o'
	env['ui_PATTERN'] = 'ui_%s.h'
	env['QT_LRELEASE_FLAGS'] = ['-silent']

	vars_debug = [a+'_debug' for a in vars]

	try:
		conf.find_program('pkg-config', var='pkgconfig', path_list=paths, mandatory=True)

	except Configure.ConfigurationError:

		for lib in vars_debug+vars:
			uselib = lib.upper()

			d = (lib.find('_debug') > 0) and 'd' or ''

			# original author seems to prefer static to shared libraries
			for (pat, kind) in ((conf.env.staticlib_PATTERN, 'STATIC'), (conf.env.shlib_PATTERN, '')):

				conf.check_message_1('Checking for %s %s' % (lib, kind))

				for ext in ['', '4']:
					path = os.path.join(qtlibs, pat % (lib + d + ext))
					if os.path.exists(path):
						env.append_unique(kind + 'LIB_' + uselib, lib + d + ext)
						conf.check_message_2('ok ' + path, 'GREEN')
						break
					path = os.path.join(qtbin, pat % (lib + d + ext))
					if os.path.exists(path):
						env.append_unique(kind + 'LIB_' + uselib, lib + d + ext)
						conf.check_message_2('ok ' + path, 'GREEN')
						break
				else:
					conf.check_message_2('not found', 'YELLOW')
					continue
				break

			env.append_unique('LIBPATH_' + uselib, qtlibs)
			env.append_unique('CPPPATH_' + uselib, qtincludes)
			env.append_unique('CPPPATH_' + uselib, qtincludes + os.sep + lib)
	else:
		for i in vars_debug+vars:
			try:
				conf.check_cfg(package=i, args='--cflags --libs --silence-errors', path=conf.env.pkgconfig)
			except ValueError:
				pass

	# the libpaths are set nicely, unfortunately they make really long command-lines
	# remove the qtcore ones from qtgui, etc
	def process_lib(vars_, coreval):
		for d in vars_:
			var = d.upper()
			if var == 'QTCORE': continue

			value = env['LIBPATH_'+var]
			if value:
				core = env[coreval]
				accu = []
				for lib in value:
					if lib in core: continue
					accu.append(lib)
				env['LIBPATH_'+var] = accu

	process_lib(vars, 'LIBPATH_QTCORE')
	process_lib(vars_debug, 'LIBPATH_QTCORE_DEBUG')

	# rpath if wanted
	want_rpath = getattr(Options.options, 'want_rpath', 1)
	if want_rpath:
		def process_rpath(vars_, coreval):
			for d in vars_:
				var = d.upper()
				value = env['LIBPATH_'+var]
				if value:
					core = env[coreval]
					accu = []
					for lib in value:
						if var != 'QTCORE':
							if lib in core:
								continue
						accu.append('-Wl,--rpath='+lib)
					env['RPATH_'+var] = accu
		process_rpath(vars, 'LIBPATH_QTCORE')
		process_rpath(vars_debug, 'LIBPATH_QTCORE_DEBUG')

	env['QTLOCALE'] = str(env['PREFIX'])+'/share/locale'

def detect(conf):
	detect_qt4(conf)

def set_options(opt):
	opt.add_option('--want-rpath', type='int', default=1, dest='want_rpath', help='set rpath to 1 or 0 [Default 1]')

	opt.add_option('--header-ext',
		type='string',
		default='',
		help='header extension for moc files',
		dest='qt_header_ext')

	for i in 'qtdir qtbin qtlibs'.split():
		opt.add_option('--'+i, type='string', default='', dest=i)

	if sys.platform == "darwin":
		opt.add_option('--no-qt4-framework', action="store_false", help='do not use the framework version of Qt4 in OS X', dest='use_qt4_osxframework',default=True)

	opt.add_option('--translate', action="store_true", help="collect translation strings", dest="trans_qt4", default=False)

