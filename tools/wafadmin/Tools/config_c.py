#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005-2008 (ita)

"""
c/c++ configuration routines
"""

import os, imp, sys, shlex, shutil
from Utils import md5
import Build, Utils, Configure, Task, Options, Logs, TaskGen
from Constants import *
from Configure import conf, conftest

cfg_ver = {
	'atleast-version': '>=',
	'exact-version': '==',
	'max-version': '<=',
}

SNIP1 = '''
	int main() {
	void *p;
	p=(void*)(%s);
	return 0;
}
'''

SNIP2 = '''
int main() {
	if ((%(type_name)s *) 0) return 0;
	if (sizeof (%(type_name)s)) return 0;
}
'''

SNIP3 = '''
int main() {
	return 0;
}
'''

def parse_flags(line, uselib, env):
	"""pkg-config still has bugs on some platforms, and there are many -config programs, parsing flags is necessary :-/"""

	lst = shlex.split(line)
	while lst:
		x = lst.pop(0)
		st = x[:2]
		ot = x[2:]

		if st == '-I' or st == '/I':
			if not ot: ot = lst.pop(0)
			env.append_unique('CPPPATH_' + uselib, ot)
		elif st == '-D':
			if not ot: ot = lst.pop(0)
			env.append_unique('CXXDEFINES_' + uselib, ot)
			env.append_unique('CCDEFINES_' + uselib, ot)
		elif st == '-l':
			if not ot: ot = lst.pop(0)
			env.append_unique('LIB_' + uselib, ot)
		elif st == '-L':
			if not ot: ot = lst.pop(0)
			env.append_unique('LIBPATH_' + uselib, ot)
		elif x == '-pthread' or x.startswith('+'):
			env.append_unique('CCFLAGS_' + uselib, x)
			env.append_unique('CXXFLAGS_' + uselib, x)
			env.append_unique('LINKFLAGS_' + uselib, x)
		elif x == '-framework':
			env.append_unique('FRAMEWORK_' + uselib, lst.pop(0))
		elif x.startswith('-F'):
			env.append_unique('FRAMEWORKPATH_' + uselib, x[2:])
		elif x.startswith('-std'):
			env.append_unique('CCFLAGS_' + uselib, x)
			env.append_unique('LINKFLAGS_' + uselib, x)
		elif x.startswith('-Wl'):
			env.append_unique('LINKFLAGS_' + uselib, x)
		elif x.startswith('-m') or x.startswith('-f'):
			env.append_unique('CCFLAGS_' + uselib, x)
			env.append_unique('CXXFLAGS_' + uselib, x)

@conf
def ret_msg(self, f, kw):
	"""execute a function, when provided"""
	if isinstance(f, str):
		return f
	return f(kw)

@conf
def validate_cfg(self, kw):
	if not 'path' in kw:
		kw['path'] = 'pkg-config --errors-to-stdout --print-errors'

	# pkg-config version
	if 'atleast_pkgconfig_version' in kw:
		if not 'msg' in kw:
			kw['msg'] = 'Checking for pkg-config version >= %s' % kw['atleast_pkgconfig_version']
		return

	# pkg-config --modversion
	if 'modversion' in kw:
		return

	if 'variables' in kw:
		if not 'msg' in kw:
			kw['msg'] = 'Checking for %s variables' % kw['package']
		return

	# checking for the version of a module, for the moment, one thing at a time
	for x in cfg_ver.keys():
		y = x.replace('-', '_')
		if y in kw:
			if not 'package' in kw:
				raise ValueError('%s requires a package' % x)

			if not 'msg' in kw:
				kw['msg'] = 'Checking for %s %s %s' % (kw['package'], cfg_ver[x], kw[y])
			return

	if not 'msg' in kw:
		kw['msg'] = 'Checking for %s' % (kw['package'] or kw['path'])
	if not 'okmsg' in kw:
		kw['okmsg'] = 'yes'
	if not 'errmsg' in kw:
		kw['errmsg'] = 'not found'

@conf
def cmd_and_log(self, cmd, kw):
	Logs.debug('runner: %s\n' % cmd)
	if self.log:
		self.log.write('%s\n' % cmd)

	try:
		p = Utils.pproc.Popen(cmd, stdout=Utils.pproc.PIPE, stderr=Utils.pproc.PIPE, shell=True)
		(out, err) = p.communicate()
	except OSError, e:
		self.log.write('error %r' % e)
		self.fatal(str(e))

	out = str(out)
	err = str(err)

	if self.log:
		self.log.write(out)
		self.log.write(err)

	if p.returncode:
		if not kw.get('errmsg', ''):
			if kw.get('mandatory', False):
				kw['errmsg'] = out.strip()
			else:
				kw['errmsg'] = 'no'
		self.fatal('fail')
	return out

@conf
def exec_cfg(self, kw):

	# pkg-config version
	if 'atleast_pkgconfig_version' in kw:
		cmd = '%s --atleast-pkgconfig-version=%s' % (kw['path'], kw['atleast_pkgconfig_version'])
		self.cmd_and_log(cmd, kw)
		if not 'okmsg' in kw:
			kw['okmsg'] = 'yes'
		return

	# checking for the version of a module
	for x in cfg_ver:
		y = x.replace('-', '_')
		if y in kw:
			self.cmd_and_log('%s --%s=%s %s' % (kw['path'], x, kw[y], kw['package']), kw)
			if not 'okmsg' in kw:
				kw['okmsg'] = 'yes'
			self.define(self.have_define(kw.get('uselib_store', kw['package'])), 1, 0)
			break

	# retrieving the version of a module
	if 'modversion' in kw:
		version = self.cmd_and_log('%s --modversion %s' % (kw['path'], kw['modversion']), kw).strip()
		self.define('%s_VERSION' % Utils.quote_define_name(kw.get('uselib_store', kw['modversion'])), version)
		return version

	# retrieving variables of a module
	if 'variables' in kw:
		env = kw.get('env', self.env)
		uselib = kw.get('uselib_store', kw['package'].upper())
		vars = Utils.to_list(kw['variables'])
		for v in vars:
			val = self.cmd_and_log('%s --variable=%s %s' % (kw['path'], v, kw['package']), kw).strip()
			var = '%s_%s' % (uselib, v)
			env[var] = val
		if not 'okmsg' in kw:
			kw['okmsg'] = 'yes'
		return

	lst = [kw['path']]
	for key, val in kw.get('define_variable', {}).iteritems():
		lst.append('--define-variable=%s=%s' % (key, val))

	lst.append(kw.get('args', ''))
	lst.append(kw['package'])

	# so we assume the command-line will output flags to be parsed afterwards
	cmd = ' '.join(lst)
	ret = self.cmd_and_log(cmd, kw)
	if not 'okmsg' in kw:
		kw['okmsg'] = 'yes'

	self.define(self.have_define(kw.get('uselib_store', kw['package'])), 1, 0)
	parse_flags(ret, kw.get('uselib_store', kw['package'].upper()), kw.get('env', self.env))
	return ret

@conf
def check_cfg(self, *k, **kw):
	"""
	for pkg-config mostly, but also all the -config tools
	conf.check_cfg(path='mpicc', args='--showme:compile --showme:link', package='', uselib_store='OPEN_MPI')
	conf.check_cfg(package='dbus-1', variables='system_bus_default_address session_bus_services_dir')
	"""

	self.validate_cfg(kw)
	if 'msg' in kw:
		self.check_message_1(kw['msg'])
	ret = None
	try:
		ret = self.exec_cfg(kw)
	except Configure.ConfigurationError, e:
		if 'errmsg' in kw:
			self.check_message_2(kw['errmsg'], 'YELLOW')
		if 'mandatory' in kw and kw['mandatory']:
			if Logs.verbose > 1:
				raise
			else:
				self.fatal('the configuration failed (see %r)' % self.log.name)
	else:
		kw['success'] = ret
		if 'okmsg' in kw:
			self.check_message_2(self.ret_msg(kw['okmsg'], kw))

	return ret

# the idea is the following: now that we are certain
# that all the code here is only for c or c++, it is
# easy to put all the logic in one function
#
# this should prevent code duplication (ita)

# env: an optional environment (modified -> provide a copy)
# compiler: cc or cxx - it tries to guess what is best
# type: cprogram, cshlib, cstaticlib
# code: a c code to execute
# uselib_store: where to add the variables
# uselib: parameters to use for building
# define: define to set, like FOO in #define FOO, if not set, add /* #undef FOO */
# execute: True or False - will return the result of the execution

@conf
def validate_c(self, kw):
	"""validate the parameters for the test method"""

	if not 'env' in kw:
		kw['env'] = self.env.copy()

	env = kw['env']
	if not 'compiler' in kw:
		kw['compiler'] = 'cc'
		if env['CXX_NAME'] and Task.TaskBase.classes.get('cxx', None):
			kw['compiler'] = 'cxx'
			if not self.env['CXX']:
				self.fatal('a c++ compiler is required')
		else:
			if not self.env['CC']:
				self.fatal('a c compiler is required')

	if not 'type' in kw:
		kw['type'] = 'cprogram'

	assert not(kw['type'] != 'cprogram' and kw.get('execute', 0)), 'can only execute programs'


	#if kw['type'] != 'program' and kw.get('execute', 0):
	#	raise ValueError, 'can only execute programs'

	def to_header(dct):
		if 'header_name' in dct:
			dct = Utils.to_list(dct['header_name'])
			return ''.join(['#include <%s>\n' % x for x in dct])
		return ''

	# set the file name
	if not 'compile_mode' in kw:
		kw['compile_mode'] = (kw['compiler'] == 'cxx') and 'cxx' or 'cc'

	if not 'compile_filename' in kw:
		kw['compile_filename'] = 'test.c' + ((kw['compile_mode'] == 'cxx') and 'pp' or '')

	#OSX
	if 'framework_name' in kw:
		try: TaskGen.task_gen.create_task_macapp
		except AttributeError: self.fatal('frameworks require the osx tool')

		fwkname = kw['framework_name']
		if not 'uselib_store' in kw:
			kw['uselib_store'] = fwkname.upper()

		if not kw.get('no_header', False):
			if not 'header_name' in kw:
				kw['header_name'] = []
			fwk = '%s/%s.h' % (fwkname, fwkname)
			if kw.get('remove_dot_h', None):
				fwk = fwk[:-2]
			kw['header_name'] = Utils.to_list(kw['header_name']) + [fwk]

		kw['msg'] = 'Checking for framework %s' % fwkname
		kw['framework'] = fwkname
		#kw['frameworkpath'] = set it yourself

	if 'function_name' in kw:
		fu = kw['function_name']
		if not 'msg' in kw:
			kw['msg'] = 'Checking for function %s' % fu
		kw['code'] = to_header(kw) + SNIP1 % fu
		if not 'uselib_store' in kw:
			kw['uselib_store'] = fu.upper()
		if not 'define_name' in kw:
			kw['define_name'] = self.have_define(fu)

	elif 'type_name' in kw:
		tu = kw['type_name']
		if not 'msg' in kw:
			kw['msg'] = 'Checking for type %s' % tu
		if not 'header_name' in kw:
			kw['header_name'] = 'stdint.h'
		kw['code'] = to_header(kw) + SNIP2 % {'type_name' : tu}
		if not 'define_name' in kw:
			kw['define_name'] = self.have_define(tu.upper())

	elif 'header_name' in kw:
		if not 'msg' in kw:
			kw['msg'] = 'Checking for header %s' % kw['header_name']

		l = Utils.to_list(kw['header_name'])
		assert len(l)>0, 'list of headers in header_name is empty'

		kw['code'] = to_header(kw) + SNIP3

		if not 'uselib_store' in kw:
			kw['uselib_store'] = l[0].upper()

		if not 'define_name' in kw:
			kw['define_name'] = self.have_define(l[0])

	if 'lib' in kw:
		if not 'msg' in kw:
			kw['msg'] = 'Checking for library %s' % kw['lib']
		if not 'uselib_store' in kw:
			kw['uselib_store'] = kw['lib'].upper()

	if 'staticlib' in kw:
		if not 'msg' in kw:
			kw['msg'] = 'Checking for static library %s' % kw['staticlib']
		if not 'uselib_store' in kw:
			kw['uselib_store'] = kw['staticlib'].upper()

	if 'fragment' in kw:
		# an additional code fragment may be provided to replace the predefined code
		# in custom headers
		kw['code'] = kw['fragment']
		if not 'msg' in kw:
			kw['msg'] = 'Checking for custom code'
		if not 'errmsg' in kw:
			kw['errmsg'] = 'no'

	for (flagsname,flagstype) in [('cxxflags','compiler'), ('cflags','compiler'), ('linkflags','linker')]:
		if flagsname in kw:
			if not 'msg' in kw:
				kw['msg'] = 'Checking for %s flags %s' % (flagstype, kw[flagsname])
			if not 'errmsg' in kw:
				kw['errmsg'] = 'no'

	if not 'execute' in kw:
		kw['execute'] = False

	if not 'errmsg' in kw:
		kw['errmsg'] = 'not found'

	if not 'okmsg' in kw:
		kw['okmsg'] = 'yes'

	if not 'code' in kw:
		kw['code'] = SNIP3

	if not kw.get('success'): kw['success'] = None

	assert 'msg' in kw, 'invalid parameters, read http://freehackers.org/~tnagy/wafbook/single.html#config_helpers_c'

@conf
def post_check(self, *k, **kw):
	"set the variables after a test was run successfully"

	is_success = False
	if kw['execute']:
		if kw['success']:
			is_success = True
	else:
		is_success = (kw['success'] == 0)

	if 'define_name' in kw:
		if 'header_name' in kw or 'function_name' in kw or 'type_name' in kw or 'fragment' in kw:
			if kw['execute']:
				key = kw['success']
				if isinstance(key, str):
					if key:
						self.define(kw['define_name'], key, quote=kw.get('quote', 1))
					else:
						self.define_cond(kw['define_name'], True)
				else:
					self.define_cond(kw['define_name'], False)
			else:
				self.define_cond(kw['define_name'], is_success)

	if is_success and 'uselib_store' in kw:
		import cc, cxx
		for k in set(cc.g_cc_flag_vars).union(cxx.g_cxx_flag_vars):
			lk = k.lower()
			# inconsistency: includes -> CPPPATH
			if k == 'CPPPATH': lk = 'includes'
			if k == 'CXXDEFINES': lk = 'defines'
			if k == 'CCDEFINES': lk = 'defines'
			if lk in kw:
				val = kw[lk]
				# remove trailing slash
				if isinstance(val, str):
					val = val.rstrip(os.path.sep)
				self.env.append_unique(k + '_' + kw['uselib_store'], val)

@conf
def check(self, *k, **kw):
	# so this will be the generic function
	# it will be safer to use check_cxx or check_cc
	self.validate_c(kw)
	self.check_message_1(kw['msg'])
	ret = None
	try:
		ret = self.run_c_code(*k, **kw)
	except Configure.ConfigurationError, e:
		self.check_message_2(kw['errmsg'], 'YELLOW')
		if 'mandatory' in kw and kw['mandatory']:
			if Logs.verbose > 1:
				raise
			else:
				self.fatal('the configuration failed (see %r)' % self.log.name)
	else:
		kw['success'] = ret
		self.check_message_2(self.ret_msg(kw['okmsg'], kw))

	self.post_check(*k, **kw)
	if not kw.get('execute', False):
		return ret == 0
	return ret

@conf
def run_c_code(self, *k, **kw):
	test_f_name = kw['compile_filename']

	k = 0
	while k < 10000:
		# make certain to use a fresh folder - necessary for win32
		dir = os.path.join(self.blddir, '.conf_check_%d' % k)

		# if the folder already exists, remove it
		try:
			shutil.rmtree(dir)
		except OSError:
			pass

		try:
			os.stat(dir)
		except OSError:
			break

		k += 1

	try:
		os.makedirs(dir)
	except:
		self.fatal('cannot create a configuration test folder %r' % dir)

	try:
		os.stat(dir)
	except:
		self.fatal('cannot use the configuration test folder %r' % dir)

	bdir = os.path.join(dir, 'testbuild')

	if not os.path.exists(bdir):
		os.makedirs(bdir)

	env = kw['env']

	dest = open(os.path.join(dir, test_f_name), 'w')
	dest.write(kw['code'])
	dest.close()

	back = os.path.abspath('.')

	bld = Build.BuildContext()
	bld.log = self.log
	bld.all_envs.update(self.all_envs)
	bld.all_envs['default'] = env
	bld.lst_variants = bld.all_envs.keys()
	bld.load_dirs(dir, bdir)

	os.chdir(dir)

	bld.rescan(bld.srcnode)

	if not 'features' in kw:
		# conf.check(features='cc cprogram pyext', ...)
		kw['features'] = [kw['compile_mode'], kw['type']] # "cprogram cc"

	o = bld(features=kw['features'], source=test_f_name, target='testprog')

	for k, v in kw.iteritems():
		setattr(o, k, v)

	self.log.write("==>\n%s\n<==\n" % kw['code'])

	# compile the program
	try:
		bld.compile()
	except Utils.WafError:
		ret = Utils.ex_stack()
	else:
		ret = 0

	# chdir before returning
	os.chdir(back)

	if ret:
		self.log.write('command returned %r' % ret)
		self.fatal(str(ret))

	# if we need to run the program, try to get its result
	# keep the name of the program to execute
	if kw['execute']:
		lastprog = o.link_task.outputs[0].abspath(env)

		args = Utils.to_list(kw.get('exec_args', []))
		proc = Utils.pproc.Popen([lastprog] + args, stdout=Utils.pproc.PIPE, stderr=Utils.pproc.PIPE)
		(out, err) = proc.communicate()
		w = self.log.write
		w(str(out))
		w('\n')
		w(str(err))
		w('\n')
		w('returncode %r' % proc.returncode)
		w('\n')
		if proc.returncode:
			self.fatal(Utils.ex_stack())
		ret = out

	return ret

@conf
def check_cxx(self, *k, **kw):
	kw['compiler'] = 'cxx'
	return self.check(*k, **kw)

@conf
def check_cc(self, *k, **kw):
	kw['compiler'] = 'cc'
	return self.check(*k, **kw)

@conf
def define(self, define, value, quote=1):
	"""store a single define and its state into an internal list for later
	   writing to a config header file.  Value can only be
	   a string or int; other types not supported.  String
	   values will appear properly quoted in the generated
	   header file."""
	assert define and isinstance(define, str)

	# ordered_dict is for writing the configuration header in order
	tbl = self.env[DEFINES] or Utils.ordered_dict()

	# the user forgot to tell if the value is quoted or not
	if isinstance(value, str):
		if quote:
			tbl[define] = '"%s"' % repr('"'+value)[2:-1].replace('"', '\\"')
		else:
			tbl[define] = value
	elif isinstance(value, int):
		tbl[define] = value
	else:
		raise TypeError('define %r -> %r must be a string or an int' % (define, value))

	# add later to make reconfiguring faster
	self.env[DEFINES] = tbl
	self.env[define] = value # <- not certain this is necessary

@conf
def undefine(self, define):
	"""store a single define and its state into an internal list
	   for later writing to a config header file"""
	assert define and isinstance(define, str)

	tbl = self.env[DEFINES] or Utils.ordered_dict()

	value = UNDEFINED
	tbl[define] = value

	# add later to make reconfiguring faster
	self.env[DEFINES] = tbl
	self.env[define] = value

@conf
def define_cond(self, name, value):
	"""Conditionally define a name.
	Formally equivalent to: if value: define(name, 1) else: undefine(name)"""
	if value:
		self.define(name, 1)
	else:
		self.undefine(name)

@conf
def is_defined(self, key):
	defines = self.env[DEFINES]
	if not defines:
		return False
	try:
		value = defines[key]
	except KeyError:
		return False
	else:
		return value != UNDEFINED

@conf
def get_define(self, define):
	"get the value of a previously stored define"
	try: return self.env[DEFINES][define]
	except KeyError: return None

@conf
def have_define(self, name):
	"prefix the define with 'HAVE_' and make sure it has valid characters."
	return self.__dict__.get('HAVE_PAT', 'HAVE_%s') % Utils.quote_define_name(name)

@conf
def write_config_header(self, configfile='', env='', guard='', top=False):
	"save the defines into a file"
	if not configfile: configfile = WAF_CONFIG_H
	waf_guard = guard or '_%s_WAF' % Utils.quote_define_name(configfile)

	# configfile -> absolute path
	# there is a good reason to concatenate first and to split afterwards
	if not env: env = self.env
	if top:
		diff = ''
	else:
		diff = Utils.diff_path(self.srcdir, self.curdir)
	full = os.sep.join([self.blddir, env.variant(), diff, configfile])
	full = os.path.normpath(full)
	(dir, base) = os.path.split(full)

	try: os.makedirs(dir)
	except: pass

	dest = open(full, 'w')
	dest.write('/* Configuration header created by Waf - do not edit */\n')
	dest.write('#ifndef %s\n#define %s\n\n' % (waf_guard, waf_guard))

	dest.write(self.get_config_header())

	# config files are not removed on "waf clean"
	env.append_unique(CFG_FILES, os.path.join(diff, configfile))

	dest.write('\n#endif /* %s */\n' % waf_guard)
	dest.close()

@conf
def get_config_header(self):
	"""Fill-in the contents of the config header. Override when you need to write your own config header."""
	config_header = []

	tbl = self.env[DEFINES] or Utils.ordered_dict()
	for key in tbl.allkeys:
		value = tbl[key]
		if value is None:
			config_header.append('#define %s' % key)
		elif value is UNDEFINED:
			config_header.append('/* #undef %s */' % key)
		else:
			config_header.append('#define %s %s' % (key, value))
	return "\n".join(config_header)

@conftest
def find_cpp(conf):
	v = conf.env
	cpp = None
	if v['CPP']: cpp = v['CPP']
	elif 'CPP' in conf.environ: cpp = conf.environ['CPP']
	if not cpp: cpp = conf.find_program('cpp', var='CPP')
	if not cpp: cpp = v['CC']
	if not cpp: cpp = v['CXX']
	v['CPP'] = cpp

@conftest
def cc_add_flags(conf):
	conf.add_os_flags('CFLAGS', 'CCFLAGS')
	conf.add_os_flags('CPPFLAGS')

@conftest
def cxx_add_flags(conf):
	conf.add_os_flags('CXXFLAGS')
	conf.add_os_flags('CPPFLAGS')

@conftest
def link_add_flags(conf):
	conf.add_os_flags('LINKFLAGS')
	conf.add_os_flags('LDFLAGS', 'LINKFLAGS')

@conftest
def cc_load_tools(conf):
	conf.check_tool('cc')

@conftest
def cxx_load_tools(conf):
	conf.check_tool('cxx')

