#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005-2008 (ita)

"base for all c/c++ programs and libraries"

import os, sys, re
import TaskGen, Task, Utils, preproc, Logs, Build, Options
from Logs import error, debug, warn
from Utils import md5
from TaskGen import taskgen, after, before, feature
from Constants import *
from Configure import conftest
try:
	from cStringIO import StringIO
except ImportError:
	from io import StringIO

import config_c # <- necessary for the configuration, do not touch

USE_TOP_LEVEL = False

def get_cc_version(conf, cc, gcc=False, icc=False):

	cmd = cc + ['-dM', '-E', '-']
	try:
		p = Utils.pproc.Popen(cmd, stdin=Utils.pproc.PIPE, stdout=Utils.pproc.PIPE, stderr=Utils.pproc.PIPE)
		p.stdin.write('\n')
		out = p.communicate()[0]
	except:
		conf.fatal('could not determine the compiler version %r' % cmd)

	# PY3K: do not touch
	out = str(out)

	if gcc:
		if out.find('__INTEL_COMPILER') >= 0:
			conf.fatal('The intel compiler pretends to be gcc')
		if out.find('__GNUC__') < 0:
			conf.fatal('Could not determine the compiler type')

	if icc and out.find('__INTEL_COMPILER') < 0:
		conf.fatal('Not icc/icpc')

	k = {}
	if icc or gcc:
		out = out.split('\n')
		import shlex

		for line in out:
			lst = shlex.split(line)
			if len(lst)>2:
				key = lst[1]
				val = lst[2]
				k[key] = val

		def isD(var):
			return var in k

		def isT(var):
			return var in k and k[var] != '0'

		# Some documentation is available at http://predef.sourceforge.net
		# The names given to DEST_OS must match what Utils.unversioned_sys_platform() returns.
		mp1 = {
			'__linux__'   : 'linux',
			'__GNU__'     : 'hurd',
			'__FreeBSD__' : 'freebsd',
			'__NetBSD__'  : 'netbsd',
			'__OpenBSD__' : 'openbsd',
			'__sun'       : 'sunos',
			'__hpux'      : 'hpux',
			'__sgi'       : 'irix',
			'_AIX'        : 'aix',
			'__CYGWIN__'  : 'cygwin',
			'__MSYS__'    : 'msys',
			'_UWIN'       : 'uwin',
			'_WIN64'      : 'win32',
			'_WIN32'      : 'win32',
			}

		for i in mp1:
			if isD(i):
				conf.env.DEST_OS = mp1[i]
				break
		else:
			if isD('__APPLE__') and isD('__MACH__'):
				conf.env.DEST_OS = 'darwin'
			elif isD('__unix__'): # unix must be tested last as it's a generic fallback
				conf.env.DEST_OS = 'generic'

		if isD('__ELF__'):
			conf.env.DEST_BINFMT = 'elf'

		mp2 = {
				'__x86_64__'  : 'x86_64',
				'__i386__'    : 'x86',
				'__ia64__'    : 'ia',
				'__mips__'    : 'mips',
				'__sparc__'   : 'sparc',
				'__alpha__'   : 'alpha',
				'__arm__'     : 'arm',
				'__hppa__'    : 'hppa',
				'__powerpc__' : 'powerpc',
				}
		for i in mp2:
			if isD(i):
				conf.env.DEST_CPU = mp2[i]
				break

		debug('ccroot: dest platform: ' + ' '.join([conf.env[x] or '?' for x in ('DEST_OS', 'DEST_BINFMT', 'DEST_CPU')]))
		conf.env['CC_VERSION'] = (k['__GNUC__'], k['__GNUC_MINOR__'], k['__GNUC_PATCHLEVEL__'])
	return k

class DEBUG_LEVELS:
	"""Will disappear in waf 1.6"""
	ULTRADEBUG = "ultradebug"
	DEBUG = "debug"
	RELEASE = "release"
	OPTIMIZED = "optimized"
	CUSTOM = "custom"

	ALL = [ULTRADEBUG, DEBUG, RELEASE, OPTIMIZED, CUSTOM]

def scan(self):
	"look for .h the .cpp need"
	debug('ccroot: _scan_preprocessor(self, node, env, path_lst)')

	# TODO waf 1.6 - assume the default input has exactly one file

	if len(self.inputs) == 1:
		node = self.inputs[0]
		(nodes, names) = preproc.get_deps(node, self.env, nodepaths = self.env['INC_PATHS'])
		if Logs.verbose:
			debug('deps: deps for %s: %r; unresolved %r', str(node), nodes, names)
		return (nodes, names)

	all_nodes = []
	all_names = []
	seen = set()
	for node in self.inputs:
		(nodes, names) = preproc.get_deps(node, self.env, nodepaths = self.env['INC_PATHS'])
		if Logs.verbose:
			debug('deps: deps for %s: %r; unresolved %r', str(node), nodes, names)
		for x in nodes:
			if id(x) in seen: continue
			seen.add(id(x))
			all_nodes.append(x)
		for x in names:
			if not x in all_names:
				all_names.append(x)
	return (all_nodes, all_names)

class ccroot_abstract(TaskGen.task_gen):
	"Parent class for programs and libraries in languages c, c++ and moc (Qt)"
	def __init__(self, *k, **kw):
		# COMPAT remove in waf 1.6 TODO
		if len(k) > 1:
			k = list(k)
			if k[1][0] != 'c':
				k[1] = 'c' + k[1]
		TaskGen.task_gen.__init__(self, *k, **kw)

def get_target_name(self):
	tp = 'program'
	for x in self.features:
		if x in ['cshlib', 'cstaticlib']:
			tp = x.lstrip('c')

	pattern = self.env[tp + '_PATTERN']
	if not pattern: pattern = '%s'

	dir, name = os.path.split(self.target)

	if self.env.DEST_BINFMT == 'pe' and getattr(self, 'vnum', None) and 'cshlib' in self.features:
		# include the version in the dll file name,
		# the import lib file name stays unversionned.
		name = name + '-' + self.vnum.split('.')[0]

	return os.path.join(dir, pattern % name)

@feature('cc', 'cxx')
@before('apply_core')
def default_cc(self):
	"""compiled_tasks attribute must be set before the '.c->.o' tasks can be created"""
	Utils.def_attrs(self,
		includes = '',
		defines= '',
		rpaths = '',
		uselib = '',
		uselib_local = '',
		add_objects = '',
		p_flag_vars = [],
		p_type_vars = [],
		compiled_tasks = [],
		link_task = None)

	# The only thing we need for cross-compilation is DEST_BINFMT.
	# At some point, we may reach a case where DEST_BINFMT is not enough, but for now it's sufficient.
	# Currently, cross-compilation is auto-detected only for the gnu and intel compilers.
	if not self.env.DEST_BINFMT:
		# Infer the binary format from the os name.
		self.env.DEST_BINFMT = Utils.unversioned_sys_platform_to_binary_format(
			self.env.DEST_OS or Utils.unversioned_sys_platform())

	if not self.env.BINDIR: self.env.BINDIR = Utils.subst_vars('${PREFIX}/bin', self.env)
	if not self.env.LIBDIR: self.env.LIBDIR = Utils.subst_vars('${PREFIX}/lib${LIB_EXT}', self.env)

@feature('cprogram', 'dprogram', 'cstaticlib', 'dstaticlib', 'cshlib', 'dshlib')
def apply_verif(self):
	"""no particular order, used for diagnostic"""
	if not (self.source or getattr(self, 'add_objects', None) or getattr(self, 'uselib_local', None) or getattr(self, 'obj_files', None)):
		raise Utils.WafError('no source files specified for %s' % self)
	if not self.target:
		raise Utils.WafError('no target for %s' % self)

# TODO reference the d programs, shlibs in d.py, not here

@feature('cprogram', 'dprogram')
@after('default_cc')
@before('apply_core')
def vars_target_cprogram(self):
	self.default_install_path = self.env.BINDIR
	self.default_chmod = O755

@after('default_cc')
@feature('cshlib', 'dshlib')
@before('apply_core')
def vars_target_cshlib(self):
	if self.env.DEST_BINFMT == 'pe':
		#   set execute bit on libs to avoid 'permission denied' (issue 283)
		self.default_chmod = O755
		self.default_install_path = self.env.BINDIR
	else:
		self.default_install_path = self.env.LIBDIR

@feature('cprogram', 'dprogram', 'cstaticlib', 'dstaticlib', 'cshlib', 'dshlib')
@after('apply_link', 'vars_target_cprogram', 'vars_target_cshlib')
def default_link_install(self):
	"""you may kill this method to inject your own installation for the first element
	any other install should only process its own nodes and not those from the others"""
	if self.install_path:
		self.bld.install_files(self.install_path, self.link_task.outputs[0], env=self.env, chmod=self.chmod)

@feature('cc', 'cxx')
@after('apply_type_vars', 'apply_lib_vars', 'apply_core')
def apply_incpaths(self):
	"""used by the scanner
	after processing the uselib for CPPPATH
	after apply_core because some processing may add include paths
	"""
	lst = []
	# TODO move the uselib processing out of here
	for lib in self.to_list(self.uselib):
		for path in self.env['CPPPATH_' + lib]:
			if not path in lst:
				lst.append(path)
	if preproc.go_absolute:
		for path in preproc.standard_includes:
			if not path in lst:
				lst.append(path)

	for path in self.to_list(self.includes):
		if not path in lst:
			if preproc.go_absolute or not os.path.isabs(path):
				lst.append(path)
			else:
				self.env.prepend_value('CPPPATH', path)

	for path in lst:
		node = None
		if os.path.isabs(path):
			if preproc.go_absolute:
				node = self.bld.root.find_dir(path)
		elif path[0] == '#':
			node = self.bld.srcnode
			if len(path) > 1:
				node = node.find_dir(path[1:])
		else:
			node = self.path.find_dir(path)

		if node:
			self.env.append_value('INC_PATHS', node)

	# TODO WAF 1.6
	if USE_TOP_LEVEL:
		self.env.append_value('INC_PATHS', self.bld.srcnode)

@feature('cc', 'cxx')
@after('init_cc', 'init_cxx')
@before('apply_lib_vars')
def apply_type_vars(self):
	"""before apply_lib_vars because we modify uselib
	after init_cc and init_cxx because web need p_type_vars
	"""
	for x in self.features:
		if not x in ['cprogram', 'cstaticlib', 'cshlib']:
			continue
		x = x.lstrip('c')

		# if the type defines uselib to add, add them
		st = self.env[x + '_USELIB']
		if st: self.uselib = self.uselib + ' ' + st

		# each compiler defines variables like 'shlib_CXXFLAGS', 'shlib_LINKFLAGS', etc
		# so when we make a task generator of the type shlib, CXXFLAGS are modified accordingly
		for var in self.p_type_vars:
			compvar = '%s_%s' % (x, var)
			#print compvar
			value = self.env[compvar]
			if value: self.env.append_value(var, value)

@feature('cprogram', 'cshlib', 'cstaticlib')
@after('apply_core')
def apply_link(self):
	"""executes after apply_core for collecting 'compiled_tasks'
	use a custom linker if specified (self.link='name-of-custom-link-task')"""
	link = getattr(self, 'link', None)
	if not link:
		if 'cstaticlib' in self.features: link = 'static_link'
		elif 'cxx' in self.features: link = 'cxx_link'
		else: link = 'cc_link'

	tsk = self.create_task(link)
	outputs = [t.outputs[0] for t in self.compiled_tasks]
	tsk.set_inputs(outputs)
	tsk.set_outputs(self.path.find_or_declare(get_target_name(self)))

	self.link_task = tsk

@feature('cc', 'cxx')
@after('apply_link', 'init_cc', 'init_cxx', 'apply_core')
def apply_lib_vars(self):
	"""after apply_link because of 'link_task'
	after default_cc because of the attribute 'uselib'"""

	# after 'apply_core' in case if 'cc' if there is no link

	env = self.env

	# 1. the case of the libs defined in the project (visit ancestors first)
	# the ancestors external libraries (uselib) will be prepended
	self.uselib = self.to_list(self.uselib)
	names = self.to_list(self.uselib_local)

	seen = set([])
	tmp = Utils.deque(names) # consume a copy of the list of names
	while tmp:
		lib_name = tmp.popleft()
		# visit dependencies only once
		if lib_name in seen:
			continue

		y = self.name_to_obj(lib_name)
		if not y:
			raise Utils.WafError('object %r was not found in uselib_local (required by %r)' % (lib_name, self.name))
		y.post()
		seen.add(lib_name)

		# object has ancestors to process (shared libraries): add them to the end of the list
		if getattr(y, 'uselib_local', None):
			lst = y.to_list(y.uselib_local)
			if 'cshlib' in y.features or 'cprogram' in y.features:
				lst = [x for x in lst if not 'cstaticlib' in self.name_to_obj(x).features]
			tmp.extend(lst)

		# link task and flags
		if getattr(y, 'link_task', None):

			link_name = y.target[y.target.rfind(os.sep) + 1:]
			if 'cstaticlib' in y.features:
				env.append_value('STATICLIB', link_name)
			elif 'cshlib' in y.features or 'cprogram' in y.features:
				# WARNING some linkers can link against programs
				env.append_value('LIB', link_name)

			# the order
			self.link_task.set_run_after(y.link_task)

			# for the recompilation
			dep_nodes = getattr(self.link_task, 'dep_nodes', [])
			self.link_task.dep_nodes = dep_nodes + y.link_task.outputs

			# add the link path too
			tmp_path = y.link_task.outputs[0].parent.bldpath(self.env)
			if not tmp_path in env['LIBPATH']: env.prepend_value('LIBPATH', tmp_path)

		# add ancestors uselib too - but only propagate those that have no staticlib
		for v in self.to_list(y.uselib):
			if not env['STATICLIB_' + v]:
				if not v in self.uselib:
					self.uselib.insert(0, v)

		# if the library task generator provides 'export_incdirs', add to the include path
		# the export_incdirs must be a list of paths relative to the other library
		if getattr(y, 'export_incdirs', None):
			for x in self.to_list(y.export_incdirs):
				node = y.path.find_dir(x)
				if not node:
					raise Utils.WafError('object %r: invalid folder %r in export_incdirs' % (y.target, x))
				self.env.append_unique('INC_PATHS', node)

	# 2. the case of the libs defined outside
	for x in self.uselib:
		for v in self.p_flag_vars:
			val = self.env[v + '_' + x]
			if val: self.env.append_value(v, val)

@feature('cprogram', 'cstaticlib', 'cshlib')
@after('init_cc', 'init_cxx', 'apply_link')
def apply_objdeps(self):
	"add the .o files produced by some other object files in the same manner as uselib_local"
	if not getattr(self, 'add_objects', None): return

	seen = []
	names = self.to_list(self.add_objects)
	while names:
		x = names[0]

		# visit dependencies only once
		if x in seen:
			names = names[1:]
			continue

		# object does not exist ?
		y = self.name_to_obj(x)
		if not y:
			raise Utils.WafError('object %r was not found in uselib_local (required by add_objects %r)' % (x, self.name))

		# object has ancestors to process first ? update the list of names
		if getattr(y, 'add_objects', None):
			added = 0
			lst = y.to_list(y.add_objects)
			lst.reverse()
			for u in lst:
				if u in seen: continue
				added = 1
				names = [u]+names
			if added: continue # list of names modified, loop

		# safe to process the current object
		y.post()
		seen.append(x)

		for t in y.compiled_tasks:
			self.link_task.inputs.extend(t.outputs)

@feature('cprogram', 'cshlib', 'cstaticlib')
@after('apply_lib_vars')
def apply_obj_vars(self):
	"""after apply_lib_vars for uselib"""
	v = self.env
	lib_st           = v['LIB_ST']
	staticlib_st     = v['STATICLIB_ST']
	libpath_st       = v['LIBPATH_ST']
	staticlibpath_st = v['STATICLIBPATH_ST']
	rpath_st         = v['RPATH_ST']

	app = v.append_unique

	if v['FULLSTATIC']:
		v.append_value('LINKFLAGS', v['FULLSTATIC_MARKER'])

	for i in v['RPATH']:
		if i and rpath_st:
			app('LINKFLAGS', rpath_st % i)

	for i in v['LIBPATH']:
		app('LINKFLAGS', libpath_st % i)
		app('LINKFLAGS', staticlibpath_st % i)

	if v['STATICLIB']:
		v.append_value('LINKFLAGS', v['STATICLIB_MARKER'])
		k = [(staticlib_st % i) for i in v['STATICLIB']]
		app('LINKFLAGS', k)

	# fully static binaries ?
	if not v['FULLSTATIC']:
		if v['STATICLIB'] or v['LIB']:
			v.append_value('LINKFLAGS', v['SHLIB_MARKER'])

	app('LINKFLAGS', [lib_st % i for i in v['LIB']])

@after('apply_link')
def process_obj_files(self):
	if not hasattr(self, 'obj_files'): return
	for x in self.obj_files:
		node = self.path.find_resource(x)
		self.link_task.inputs.append(node)

@taskgen
def add_obj_file(self, file):
	"""Small example on how to link object files as if they were source
	obj = bld.create_obj('cc')
	obj.add_obj_file('foo.o')"""
	if not hasattr(self, 'obj_files'): self.obj_files = []
	if not 'process_obj_files' in self.meths: self.meths.append('process_obj_files')
	self.obj_files.append(file)

c_attrs = {
'cxxflag' : 'CXXFLAGS',
'cflag' : 'CCFLAGS',
'ccflag' : 'CCFLAGS',
'linkflag' : 'LINKFLAGS',
'ldflag' : 'LINKFLAGS',
'lib' : 'LIB',
'libpath' : 'LIBPATH',
'staticlib': 'STATICLIB',
'staticlibpath': 'STATICLIBPATH',
'rpath' : 'RPATH',
'framework' : 'FRAMEWORK',
'frameworkpath' : 'FRAMEWORKPATH'
}

@feature('cc', 'cxx')
@before('init_cxx', 'init_cc')
@before('apply_lib_vars', 'apply_obj_vars', 'apply_incpaths', 'init_cc')
def add_extra_flags(self):
	"""case and plural insensitive
	before apply_obj_vars for processing the library attributes
	"""
	for x in self.__dict__.keys():
		y = x.lower()
		if y[-1] == 's':
			y = y[:-1]
		if c_attrs.get(y, None):
			self.env.append_unique(c_attrs[y], getattr(self, x))

# ============ the code above must not know anything about import libs ==========

@feature('cshlib')
@after('apply_link', 'default_cc')
@before('apply_lib_vars', 'apply_objdeps', 'default_link_install')
def apply_implib(self):
	"""On mswindows, handle dlls and their import libs
	the .dll.a is the import lib and it is required for linking so it is installed too
	"""
	if not self.env.DEST_BINFMT == 'pe':
		return

	self.meths.remove('default_link_install')

	bindir = self.install_path
	if not bindir: return

	# install the dll in the bin dir
	dll = self.link_task.outputs[0]
	self.bld.install_files(bindir, dll, self.env, self.chmod)

	# add linker flags to generate the import lib
	implib = self.env['implib_PATTERN'] % os.path.split(self.target)[1]

	implib = dll.parent.find_or_declare(implib)
	self.link_task.outputs.append(implib)
	self.bld.install_as('${LIBDIR}/%s' % implib.name, implib, self.env)

	self.env.append_value('LINKFLAGS', (self.env['IMPLIB_ST'] % implib.bldpath(self.env)).split())

# ============ the code above must not know anything about vnum processing on unix platforms =========

@feature('cshlib')
@after('apply_link')
@before('apply_lib_vars', 'default_link_install')
def apply_vnum(self):
	"""
	libfoo.so is installed as libfoo.so.1.2.3
	"""
	if not getattr(self, 'vnum', '') or not 'cshlib' in self.features or os.name != 'posix' or self.env.DEST_BINFMT not in ('elf', 'mac-o'):
		return

	self.meths.remove('default_link_install')

	link = self.link_task
	nums = self.vnum.split('.')
	node = link.outputs[0]

	libname = node.name
	if libname.endswith('.dylib'):
		name3 = libname.replace('.dylib', '.%s.dylib' % self.vnum)
		name2 = libname.replace('.dylib', '.%s.dylib' % nums[0])
	else:
		name3 = libname + '.' + self.vnum
		name2 = libname + '.' + nums[0]

	if self.env.SONAME_ST:
		v = self.env.SONAME_ST % name2
		self.env.append_value('LINKFLAGS', v.split())

	bld = self.bld
	nums = self.vnum.split('.')

	path = self.install_path
	if not path: return

	bld.install_as(path + os.sep + name3, node, env=self.env)
	bld.symlink_as(path + os.sep + name2, name3)
	bld.symlink_as(path + os.sep + libname, name3)

	# the following task is just to enable execution from the build dir :-/
	tsk = self.create_task('vnum')
	tsk.set_inputs([node])
	tsk.set_outputs(node.parent.find_or_declare(name2))

def exec_vnum_link(self):
	path = self.outputs[0].abspath(self.env)
	try:
		os.remove(path)
	except OSError:
		pass

	try:
		os.symlink(self.inputs[0].name, path)
	except OSError:
		return 1

cls = Task.task_type_from_func('vnum', func=exec_vnum_link, ext_in='.bin', color='CYAN')
cls.quiet = 1

# ============ the --as-needed flag should added during the configuration, not at runtime =========

@conftest
def add_as_needed(conf):
	if conf.env.DEST_BINFMT == 'elf' and 'gcc' in (conf.env.CXX_NAME, conf.env.CC_NAME):
		conf.env.append_unique('LINKFLAGS', '--as-needed')

