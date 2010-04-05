#!/usr/bin/env python
# encoding: utf-8
# Carlos Rafael Giani, 2007 (dv)
# Thomas Nagy, 2007-2008 (ita)

import os, sys, re, optparse
import ccroot # <- leave this
import TaskGen, Utils, Task, Configure, Logs, Build
from Logs import debug, error
from TaskGen import taskgen, feature, after, before, extension
from Configure import conftest

EXT_D = ['.d', '.di', '.D']
D_METHS = ['apply_core', 'apply_vnum', 'apply_objdeps'] # additional d methods

def filter_comments(filename):
	txt = Utils.readf(filename)
	buf = []

	i = 0
	max = len(txt)
	while i < max:
		c = txt[i]
		# skip a string
		if c == '"':
			i += 1
			c = ''
			while i < max:
				p = c
				c = txt[i]
				i += 1
				if i == max: return buf
				if c == '"':
					cnt = 0
					while i < cnt and i < max:
						#print "cntcnt = ", str(cnt), self.txt[self.i-2-cnt]
						if txt[i-2-cnt] == '\\': cnt+=1
						else: break
					#print "cnt is ", str(cnt)
					if (cnt%2)==0: break
			i += 1
		# skip a char
		elif c == "'":
			i += 1
			if i == max: return buf
			c = txt[i]
			if c == '\\':
				i += 1
				if i == max: return buf
				c = txt[i]
				if c == 'x':
					i += 2 # skip two chars
				elif c == 'u':
					i += 4 # skip unicode chars
			i += 1
			if i == max: return buf
			c = txt[i]
			if c != '\'': error("uh-oh, invalid character")

		# skip a comment
		elif c == '/':
			if i == max: break
			c = txt[i+1]
			# eat /+ +/ comments
			if c == '+':
				i += 1
				nesting = 1
				prev = 0
				while i < max:
					c = txt[i]
					if c == '+':
						prev = 1
					elif c == '/':
						if prev:
							nesting -= 1
							if nesting == 0: break
						else:
							if i < max:
								i += 1
								c = txt[i]
								if c == '+':
									nesting += 1
							else:
								return buf
					else:
						prev = 0
					i += 1
			# eat /* */ comments
			elif c == '*':
				i += 1
				while i < max:
					c = txt[i]
					if c == '*':
						prev = 1
					elif c == '/':
						if prev: break
					else:
						prev = 0
					i += 1
			# eat // comments
			elif c == '/':
				i += 1
				c = txt[i]
				while i < max and c != '\n':
					i += 1
					c = txt[i]
		# a valid char, add it to the buffer
		else:
			buf.append(c)
		i += 1
	return buf

class d_parser(object):
	def __init__(self, env, incpaths):
		#self.code = ''
		#self.module = ''
		#self.imports = []

		self.allnames = []

		self.re_module = re.compile("module\s+([^;]+)")
		self.re_import = re.compile("import\s+([^;]+)")
		self.re_import_bindings = re.compile("([^:]+):(.*)")
		self.re_import_alias = re.compile("[^=]+=(.+)")

		self.env = env

		self.nodes = []
		self.names = []

		self.incpaths = incpaths

	def tryfind(self, filename):
		found = 0
		for n in self.incpaths:
			found = n.find_resource(filename.replace('.', '/') + '.d')
			if found:
				self.nodes.append(found)
				self.waiting.append(found)
				break
		if not found:
			if not filename in self.names:
				self.names.append(filename)

	def get_strings(self, code):
		#self.imports = []
		self.module = ''
		lst = []

		# get the module name (if present)

		mod_name = self.re_module.search(code)
		if mod_name:
			self.module = re.sub('\s+', '', mod_name.group(1)) # strip all whitespaces

		# go through the code, have a look at all import occurrences

		# first, lets look at anything beginning with "import" and ending with ";"
		import_iterator = self.re_import.finditer(code)
		if import_iterator:
			for import_match in import_iterator:
				import_match_str = re.sub('\s+', '', import_match.group(1)) # strip all whitespaces

				# does this end with an import bindings declaration?
				# (import bindings always terminate the list of imports)
				bindings_match = self.re_import_bindings.match(import_match_str)
				if bindings_match:
					import_match_str = bindings_match.group(1)
					# if so, extract the part before the ":" (since the module declaration(s) is/are located there)

				# split the matching string into a bunch of strings, separated by a comma
				matches = import_match_str.split(',')

				for match in matches:
					alias_match = self.re_import_alias.match(match)
					if alias_match:
						# is this an alias declaration? (alias = module name) if so, extract the module name
						match = alias_match.group(1)

					lst.append(match)
		return lst

	def start(self, node):
		self.waiting = [node]
		# while the stack is not empty, add the dependencies
		while self.waiting:
			nd = self.waiting.pop(0)
			self.iter(nd)

	def iter(self, node):
		path = node.abspath(self.env) # obtain the absolute path
		code = "".join(filter_comments(path)) # read the file and filter the comments
		names = self.get_strings(code) # obtain the import strings
		for x in names:
			# optimization
			if x in self.allnames: continue
			self.allnames.append(x)

			# for each name, see if it is like a node or not
			self.tryfind(x)

def scan(self):
	"look for .d/.di the .d source need"
	env = self.env
	gruik = d_parser(env, env['INC_PATHS'])
	gruik.start(self.inputs[0])

	if Logs.verbose:
		debug('deps: nodes found for %s: %s %s' % (str(self.inputs[0]), str(gruik.nodes), str(gruik.names)))
		#debug("deps found for %s: %s" % (str(node), str(gruik.deps)), 'deps')
	return (gruik.nodes, gruik.names)

def get_target_name(self):
	"for d programs and libs"
	v = self.env
	tp = 'program'
	for x in self.features:
		if x in ['dshlib', 'dstaticlib']:
			tp = x.lstrip('d')
	return v['D_%s_PATTERN' % tp] % self.target

d_params = {
'dflags': '',
'importpaths':'',
'libs':'',
'libpaths':'',
'generate_headers':False,
}

@feature('d')
@before('apply_type_vars')
def init_d(self):
	for x in d_params:
		setattr(self, x, getattr(self, x, d_params[x]))

class d_taskgen(TaskGen.task_gen):
	def __init__(self, *k, **kw):
		TaskGen.task_gen.__init__(self, *k, **kw)

		# COMPAT
		if len(k) > 1:
			self.features.append('d' + k[1])

# okay, we borrow a few methods from ccroot
TaskGen.bind_feature('d', D_METHS)

@feature('d')
@before('apply_d_libs')
def init_d(self):
	Utils.def_attrs(self,
		dflags='',
		importpaths='',
		libs='',
		libpaths='',
		uselib='',
		uselib_local='',
		generate_headers=False, # set to true if you want .di files as well as .o
		compiled_tasks=[],
		add_objects=[],
		link_task=None)

@feature('d')
@after('apply_d_link', 'init_d')
@before('apply_vnum')
def apply_d_libs(self):
	"""after apply_link because of 'link_task'
	after default_cc because of the attribute 'uselib'"""
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
			if 'dshlib' in y.features or 'dprogram' in y.features:
				lst = [x for x in lst if not 'dstaticlib' in self.name_to_obj(x).features]
			tmp.extend(lst)

		# link task and flags
		if getattr(y, 'link_task', None):

			link_name = y.target[y.target.rfind(os.sep) + 1:]
			if 'dstaticlib' in y.features or 'dshlib' in y.features:
				env.append_unique('DLINKFLAGS', env.DLIB_ST % link_name)
				env.append_unique('DLINKFLAGS', env.DLIBPATH_ST % y.link_task.outputs[0].parent.bldpath(env))

			# the order
			self.link_task.set_run_after(y.link_task)

			# for the recompilation
			dep_nodes = getattr(self.link_task, 'dep_nodes', [])
			self.link_task.dep_nodes = dep_nodes + y.link_task.outputs

		# add ancestors uselib too - but only propagate those that have no staticlib
		for v in self.to_list(y.uselib):
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

@feature('dprogram', 'dshlib', 'dstaticlib')
@after('apply_core')
def apply_d_link(self):
	link = getattr(self, 'link', None)
	if not link:
		if 'dstaticlib' in self.features: link = 'static_link'
		else: link = 'd_link'

	outputs = [t.outputs[0] for t in self.compiled_tasks]
	self.link_task = self.create_task(link, outputs, self.path.find_or_declare(get_target_name(self)))

@feature('d')
@after('apply_core')
def apply_d_vars(self):
	env = self.env
	dpath_st   = env['DPATH_ST']
	lib_st     = env['DLIB_ST']
	libpath_st = env['DLIBPATH_ST']

	importpaths = self.to_list(self.importpaths)
	libpaths = []
	libs = []
	uselib = self.to_list(self.uselib)

	for i in uselib:
		if env['DFLAGS_' + i]:
			env.append_unique('DFLAGS', env['DFLAGS_' + i])

	for x in self.features:
		if not x in ['dprogram', 'dstaticlib', 'dshlib']:
			continue
		x.lstrip('d')
		d_shlib_dflags = env['D_' + x + '_DFLAGS']
		if d_shlib_dflags:
			env.append_unique('DFLAGS', d_shlib_dflags)

	# add import paths
	for i in uselib:
		if env['DPATH_' + i]:
			for entry in self.to_list(env['DPATH_' + i]):
				if not entry in importpaths:
					importpaths.append(entry)

	# now process the import paths
	for path in importpaths:
		if os.path.isabs(path):
			env.append_unique('_DIMPORTFLAGS', dpath_st % path)
		else:
			node = self.path.find_dir(path)
			self.env.append_unique('INC_PATHS', node)
			env.append_unique('_DIMPORTFLAGS', dpath_st % node.srcpath(env))
			env.append_unique('_DIMPORTFLAGS', dpath_st % node.bldpath(env))

	# add library paths
	for i in uselib:
		if env['LIBPATH_' + i]:
			for entry in self.to_list(env['LIBPATH_' + i]):
				if not entry in libpaths:
					libpaths.append(entry)
	libpaths = self.to_list(self.libpaths) + libpaths

	# now process the library paths
	# apply same path manipulation as used with import paths
	for path in libpaths:
		if not os.path.isabs(path):
			node = self.path.find_resource(path)
			if not node:
				raise Utils.WafError('could not find libpath %r from %r' % (path, self))
			path = node.abspath(self.env)

		env.append_unique('DLINKFLAGS', libpath_st % path)

	# add libraries
	for i in uselib:
		if env['LIB_' + i]:
			for entry in self.to_list(env['LIB_' + i]):
				if not entry in libs:
					libs.append(entry)
	libs.extend(self.to_list(self.libs))

	# process user flags
	for flag in self.to_list(self.dflags):
		env.append_unique('DFLAGS', flag)

	# now process the libraries
	for lib in libs:
		env.append_unique('DLINKFLAGS', lib_st % lib)

	# add linker flags
	for i in uselib:
		dlinkflags = env['DLINKFLAGS_' + i]
		if dlinkflags:
			for linkflag in dlinkflags:
				env.append_unique('DLINKFLAGS', linkflag)

@feature('dshlib')
@after('apply_d_vars')
def add_shlib_d_flags(self):
	for linkflag in self.env['D_shlib_LINKFLAGS']:
		self.env.append_unique('DLINKFLAGS', linkflag)

@extension(EXT_D)
def d_hook(self, node):
	# create the compilation task: cpp or cc
	task = self.create_task(self.generate_headers and 'd_with_header' or 'd')
	try: obj_ext = self.obj_ext
	except AttributeError: obj_ext = '_%d.o' % self.idx

	task.inputs = [node]
	task.outputs = [node.change_ext(obj_ext)]
	self.compiled_tasks.append(task)

	if self.generate_headers:
		header_node = node.change_ext(self.env['DHEADER_ext'])
		task.outputs += [header_node]

d_str = '${D_COMPILER} ${DFLAGS} ${_DIMPORTFLAGS} ${D_SRC_F}${SRC} ${D_TGT_F}${TGT}'
d_with_header_str = '${D_COMPILER} ${DFLAGS} ${_DIMPORTFLAGS} \
${D_HDR_F}${TGT[1].bldpath(env)} \
${D_SRC_F}${SRC} \
${D_TGT_F}${TGT[0].bldpath(env)}'
link_str = '${D_LINKER} ${DLNK_SRC_F}${SRC} ${DLNK_TGT_F}${TGT} ${DLINKFLAGS}'

def override_exec(cls):
	"""stupid dmd wants -of stuck to the file name"""
	old_exec = cls.exec_command
	def exec_command(self, *k, **kw):
		if isinstance(k[0], list):
			lst = k[0]
			for i in xrange(len(lst)):
				if lst[i] == '-of':
					del lst[i]
					lst[i] = '-of' + lst[i]
					break
		return old_exec(self, *k, **kw)
	cls.exec_command = exec_command

cls = Task.simple_task_type('d', d_str, 'GREEN', before='static_link d_link', shell=False)
cls.scan = scan
override_exec(cls)

cls = Task.simple_task_type('d_with_header', d_with_header_str, 'GREEN', before='static_link d_link', shell=False)
override_exec(cls)

cls = Task.simple_task_type('d_link', link_str, color='YELLOW', shell=False)
override_exec(cls)

# for feature request #104
@taskgen
def generate_header(self, filename, install_path):
	if not hasattr(self, 'header_lst'): self.header_lst = []
	self.meths.append('process_header')
	self.header_lst.append([filename, install_path])

@before('apply_core')
def process_header(self):
	env = self.env
	for i in getattr(self, 'header_lst', []):
		node = self.path.find_resource(i[0])

		if not node:
			raise Utils.WafError('file not found on d obj '+i[0])

		task = self.create_task('d_header')
		task.set_inputs(node)
		task.set_outputs(node.change_ext('.di'))

d_header_str = '${D_COMPILER} ${D_HEADER} ${SRC}'
Task.simple_task_type('d_header', d_header_str, color='BLUE', shell=False)

@conftest
def d_platform_flags(conf):
	v = conf.env
	binfmt = v.DEST_BINFMT or Utils.unversioned_sys_platform_to_binary_format(
		v.DEST_OS or Utils.unversioned_sys_platform())
	if binfmt == 'pe':
		v['D_program_PATTERN']   = '%s.exe'
		v['D_shlib_PATTERN']     = 'lib%s.dll'
		v['D_staticlib_PATTERN'] = 'lib%s.a'
	else:
		v['D_program_PATTERN']   = '%s'
		v['D_shlib_PATTERN']     = 'lib%s.so'
		v['D_staticlib_PATTERN'] = 'lib%s.a'

# quick test #
if __name__ == "__main__":
	#Logs.verbose = 2

	try: arg = sys.argv[1]
	except IndexError: arg = "file.d"

	print("".join(filter_comments(arg)))
	# TODO
	paths = ['.']

	#gruik = filter()
	#gruik.start(arg)

	#code = "".join(gruik.buf)

	#print "we have found the following code"
	#print code

	#print "now parsing"
	#print "-------------------------------------------"
	"""
	parser_ = d_parser()
	parser_.start(arg)

	print "module: %s" % parser_.module
	print "imports: ",
	for imp in parser_.imports:
		print imp + " ",
	print
"""

