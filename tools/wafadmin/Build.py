#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"""
Dependency tree holder

The class Build holds all the info related to a build:
* file system representation (tree of Node instances)
* various cached objects (task signatures, file scan results, ..)

There is only one Build object at a time (bld singleton)
"""

import os, sys, errno, re, glob, gc, datetime, shutil
try: import cPickle
except: import pickle as cPickle
import Runner, TaskGen, Node, Scripting, Utils, Environment, Task, Logs, Options
from Logs import debug, error, info
from Constants import *

SAVED_ATTRS = 'root srcnode bldnode node_sigs node_deps raw_deps task_sigs id_nodes'.split()
"Build class members to save"

bld = None
"singleton - safe to use when Waf is not used as a library"

class BuildError(Utils.WafError):
	def __init__(self, b=None, t=[]):
		self.bld = b
		self.tasks = t
		self.ret = 1
		Utils.WafError.__init__(self, self.format_error())

	def format_error(self):
		lst = ['Build failed:']
		for tsk in self.tasks:
			txt = tsk.format_error()
			if txt: lst.append(txt)
		sep = ' '
		if len(lst) > 2:
			sep = '\n'
		return sep.join(lst)

def group_method(fun):
	"""
	sets a build context method to execute after the current group has finished executing
	this is useful for installing build files:
	* calling install_files/install_as will fail if called too early
	* people do not want to define install method in their task classes

	TODO: try it
	"""
	def f(*k, **kw):
		if not k[0].is_install:
			return False

		postpone = True
		if 'postpone' in kw:
			postpone = kw['postpone']
			del kw['postpone']

		# TODO waf 1.6 in theory there should be no reference to the TaskManager internals here
		if postpone:
			m = k[0].task_manager
			if not m.groups: m.add_group()
			m.groups[m.current_group].post_funs.append((fun, k, kw))
			if not 'cwd' in kw:
				kw['cwd'] = k[0].path
		else:
			fun(*k, **kw)
	return f

class BuildContext(Utils.Context):
	"holds the dependency tree"
	def __init__(self):

		# not a singleton, but provided for compatibility
		global bld
		bld = self

		self.task_manager = Task.TaskManager()

		# instead of hashing the nodes, we assign them a unique id when they are created
		self.id_nodes = 0
		self.idx = {}

		# map names to environments, the 'Release' must be defined
		self.all_envs = {}

		# ======================================= #
		# code for reading the scripts

		# project build directory - do not reset() from load_dirs()
		self.bdir = ''

		# the current directory from which the code is run
		# the folder changes everytime a wscript is read
		self.path = None

		# Manual dependencies.
		self.deps_man = Utils.DefaultDict(list)

		# ======================================= #
		# cache variables

		# local cache for absolute paths - cache_node_abspath[variant][node]
		self.cache_node_abspath = {}

		# list of folders that are already scanned
		# so that we do not need to stat them one more time
		self.cache_scanned_folders = {}

		# list of targets to uninstall for removing the empty folders after uninstalling
		self.uninstall = []

		# ======================================= #
		# tasks and objects

		# build dir variants (release, debug, ..)
		for v in 'cache_node_abspath task_sigs node_deps raw_deps node_sigs'.split():
			var = {}
			setattr(self, v, var)

		self.cache_dir_contents = {}

		self.all_task_gen = []
		self.task_gen_cache_names = {}
		self.cache_sig_vars = {}
		self.log = None

		self.root = None
		self.srcnode = None
		self.bldnode = None

		# bind the build context to the nodes in use
		# this means better encapsulation and no build context singleton
		class node_class(Node.Node):
			pass
		self.node_class = node_class
		self.node_class.__module__ = "Node"
		self.node_class.__name__ = "Nodu"
		self.node_class.bld = self

		self.is_install = None

	def __copy__(self):
		"nodes are not supposed to be copied"
		raise Utils.WafError('build contexts are not supposed to be cloned')

	def load(self):
		"load the cache from the disk"
		try:
			env = Environment.Environment(os.path.join(self.cachedir, 'build.config.py'))
		except (IOError, OSError):
			pass
		else:
			if env['version'] < HEXVERSION:
				raise Utils.WafError('Version mismatch! reconfigure the project')
			for t in env['tools']:
				self.setup(**t)

		try:
			gc.disable()
			f = data = None

			Node.Nodu = self.node_class

			try:
				f = open(os.path.join(self.bdir, DBFILE), 'rb')
			except (IOError, EOFError):
				# handle missing file/empty file
				pass

			try:
				if f: data = cPickle.load(f)
			except AttributeError:
				# handle file of an old Waf version
				# that has an attribute which no longer exist
				# (e.g. AttributeError: 'module' object has no attribute 'BuildDTO')
				if Logs.verbose > 1: raise

			if data:
				for x in SAVED_ATTRS: setattr(self, x, data[x])
			else:
				debug('build: Build cache loading failed')

		finally:
			if f: f.close()
			gc.enable()

	def save(self):
		"store the cache on disk, see self.load"
		gc.disable()
		self.root.__class__.bld = None

		# some people are very nervous with ctrl+c so we have to make a temporary file
		Node.Nodu = self.node_class
		db = os.path.join(self.bdir, DBFILE)
		file = open(db + '.tmp', 'wb')
		data = {}
		for x in SAVED_ATTRS: data[x] = getattr(self, x)
		cPickle.dump(data, file, -1)
		file.close()

		# do not use shutil.move
		try: os.unlink(db)
		except OSError: pass
		os.rename(db + '.tmp', db)
		self.root.__class__.bld = self
		gc.enable()

	# ======================================= #

	def clean(self):
		debug('build: clean called')

		# does not clean files created during the configuration
		precious = set([])
		for env in self.all_envs.values():
			for x in env[CFG_FILES]:
				node = self.srcnode.find_resource(x)
				if node:
					precious.add(node.id)

		def clean_rec(node):
			for x in list(node.childs.keys()):
				nd = node.childs[x]

				tp = nd.id & 3
				if tp == Node.DIR:
					clean_rec(nd)
				elif tp == Node.BUILD:
					if nd.id in precious: continue
					for env in self.all_envs.values():
						try: os.remove(nd.abspath(env))
						except OSError: pass
					node.childs.__delitem__(x)

		clean_rec(self.srcnode)

		for v in 'node_sigs node_deps task_sigs raw_deps cache_node_abspath'.split():
			setattr(self, v, {})

	def compile(self):
		"""The cache file is not written if nothing was build at all (build is up to date)"""
		debug('build: compile called')

		"""
		import cProfile, pstats
		cProfile.run("import Build\nBuild.bld.flush()", 'profi.txt')
		p = pstats.Stats('profi.txt')
		p.sort_stats('cumulative').print_stats(80)
		"""
		self.flush()
		#"""

		self.generator = Runner.Parallel(self, Options.options.jobs)

		def dw(on=True):
			if Options.options.progress_bar:
				if on: sys.stderr.write(Logs.colors.cursor_on)
				else: sys.stderr.write(Logs.colors.cursor_off)

		debug('build: executor starting')

		back = os.getcwd()
		os.chdir(self.bldnode.abspath())

		try:
			try:
				dw(on=False)
				self.generator.start()
			except KeyboardInterrupt:
				dw()
				if Runner.TaskConsumer.consumers:
					self.save()
				raise
			except Exception:
				dw()
				# do not store anything, for something bad happened
				raise
			else:
				dw()
				if Runner.TaskConsumer.consumers:
					self.save()

			if self.generator.error:
				raise BuildError(self, self.task_manager.tasks_done)

		finally:
			os.chdir(back)

	def install(self):
		"this function is called for both install and uninstall"
		debug('build: install called')

		self.flush()

		# remove empty folders after uninstalling
		if self.is_install < 0:
			lst = []
			for x in self.uninstall:
				dir = os.path.dirname(x)
				if not dir in lst: lst.append(dir)
			lst.sort()
			lst.reverse()

			nlst = []
			for y in lst:
				x = y
				while len(x) > 4:
					if not x in nlst: nlst.append(x)
					x = os.path.dirname(x)

			nlst.sort()
			nlst.reverse()
			for x in nlst:
				try: os.rmdir(x)
				except OSError: pass

	def new_task_gen(self, *k, **kw):
		if self.task_gen_cache_names:
			self.task_gen_cache_names = {}

		kw['bld'] = self
		if len(k) == 0:
			ret = TaskGen.task_gen(*k, **kw)
		else:
			cls_name = k[0]

			try: cls = TaskGen.task_gen.classes[cls_name]
			except KeyError: raise Utils.WscriptError('%s is not a valid task generator -> %s' %
				(cls_name, [x for x in TaskGen.task_gen.classes]))
			ret = cls(*k, **kw)
		return ret

	def __call__(self, *k, **kw):
		if self.task_gen_cache_names:
			self.task_gen_cache_names = {}

		kw['bld'] = self
		return TaskGen.task_gen(*k, **kw)

	def load_envs(self):
		try:
			lst = Utils.listdir(self.cachedir)
		except OSError, e:
			if e.errno == errno.ENOENT:
				raise Utils.WafError('The project was not configured: run "waf configure" first!')
			else:
				raise

		if not lst:
			raise Utils.WafError('The cache directory is empty: reconfigure the project')

		for file in lst:
			if file.endswith(CACHE_SUFFIX):
				env = Environment.Environment(os.path.join(self.cachedir, file))
				name = file[:-len(CACHE_SUFFIX)]

				self.all_envs[name] = env

		self.init_variants()

		for env in self.all_envs.values():
			for f in env[CFG_FILES]:
				newnode = self.path.find_or_declare(f)
				try:
					hash = Utils.h_file(newnode.abspath(env))
				except (IOError, AttributeError):
					error("cannot find "+f)
					hash = SIG_NIL
				self.node_sigs[env.variant()][newnode.id] = hash

		# TODO: hmmm, these nodes are removed from the tree when calling rescan()
		self.bldnode = self.root.find_dir(self.bldnode.abspath())
		self.path = self.srcnode = self.root.find_dir(self.srcnode.abspath())
		self.cwd = self.bldnode.abspath()

	def setup(self, tool, tooldir=None, funs=None):
		"setup tools for build process"
		if isinstance(tool, list):
			for i in tool: self.setup(i, tooldir)
			return

		if not tooldir: tooldir = Options.tooldir

		module = Utils.load_tool(tool, tooldir)
		if hasattr(module, "setup"): module.setup(self)

	def init_variants(self):
		debug('build: init variants')

		lstvariants = []
		for env in self.all_envs.values():
			if not env.variant() in lstvariants:
				lstvariants.append(env.variant())
		self.lst_variants = lstvariants

		debug('build: list of variants is %r', lstvariants)

		for name in lstvariants+[0]:
			for v in 'node_sigs cache_node_abspath'.split():
				var = getattr(self, v)
				if not name in var:
					var[name] = {}

	# ======================================= #
	# node and folder handling

	# this should be the main entry point
	def load_dirs(self, srcdir, blddir, load_cache=1):
		"this functions should be the start of everything"

		assert(os.path.isabs(srcdir))
		assert(os.path.isabs(blddir))

		self.cachedir = os.path.join(blddir, CACHE_DIR)

		if srcdir == blddir:
			raise Utils.WafError("build dir must be different from srcdir: %s <-> %s " % (srcdir, blddir))

		self.bdir = blddir

		# try to load the cache file, if it does not exist, nothing happens
		self.load()

		if not self.root:
			Node.Nodu = self.node_class
			self.root = Node.Nodu('', None, Node.DIR)

		if not self.srcnode:
			self.srcnode = self.root.ensure_dir_node_from_path(srcdir)
		debug('build: srcnode is %s and srcdir %s', self.srcnode.name, srcdir)

		self.path = self.srcnode

		# create this build dir if necessary
		try: os.makedirs(blddir)
		except OSError: pass

		if not self.bldnode:
			self.bldnode = self.root.ensure_dir_node_from_path(blddir)

		self.init_variants()

	def rescan(self, src_dir_node):
		"""
		look the contents of a (folder)node and update its list of childs

		The intent is to perform the following steps
		* remove the nodes for the files that have disappeared
		* remove the signatures for the build files that have disappeared
		* cache the results of os.listdir
		* create the build folder equivalent (mkdir) for each variant
		src/bar -> build/Release/src/bar, build/Debug/src/bar

		when a folder in the source directory is removed, we do not check recursively
		to remove the unused nodes. To do that, call 'waf clean' and build again.
		"""

		# do not rescan over and over again
		# TODO use a single variable in waf 1.6
		if self.cache_scanned_folders.get(src_dir_node.id, None): return
		self.cache_scanned_folders[src_dir_node.id] = True

		# TODO remove in waf 1.6
		if hasattr(self, 'repository'): self.repository(src_dir_node)

		if not src_dir_node.name and sys.platform == 'win32':
			# the root has no name, contains drive letters, and cannot be listed
			return


		# first, take the case of the source directory
		parent_path = src_dir_node.abspath()
		try:
			lst = set(Utils.listdir(parent_path))
		except OSError:
			lst = set([])

		# TODO move this at the bottom
		self.cache_dir_contents[src_dir_node.id] = lst

		# hash the existing source files, remove the others
		cache = self.node_sigs[0]
		for x in src_dir_node.childs.values():
			if x.id & 3 != Node.FILE: continue
			if x.name in lst:
				try:
					cache[x.id] = Utils.h_file(x.abspath())
				except IOError:
					raise Utils.WafError('The file %s is not readable or has become a dir' % x.abspath())
			else:
				try: del cache[x.id]
				except KeyError: pass

				del src_dir_node.childs[x.name]


		# first obtain the differences between srcnode and src_dir_node
		h1 = self.srcnode.height()
		h2 = src_dir_node.height()

		lst = []
		child = src_dir_node
		while h2 > h1:
			lst.append(child.name)
			child = child.parent
			h2 -= 1
		lst.reverse()

		# list the files in the build dirs
		try:
			for variant in self.lst_variants:
				sub_path = os.path.join(self.bldnode.abspath(), variant , *lst)
				self.listdir_bld(src_dir_node, sub_path, variant)
		except OSError:

			# listdir failed, remove the build node signatures for all variants
			for node in src_dir_node.childs.values():
				if node.id & 3 != Node.BUILD:
					continue

				for dct in self.node_sigs.values():
					if node.id in dct:
						dct.__delitem__(node.id)

				# the policy is to avoid removing nodes representing directories
				src_dir_node.childs.__delitem__(node.name)

			for variant in self.lst_variants:
				sub_path = os.path.join(self.bldnode.abspath(), variant , *lst)
				try:
					os.makedirs(sub_path)
				except OSError:
					pass

	# ======================================= #
	def listdir_src(self, parent_node):
		"""do not use, kept for compatibility"""
		pass

	def remove_node(self, node):
		"""do not use, kept for compatibility"""
		pass

	def listdir_bld(self, parent_node, path, variant):
		"""in this method we do not add timestamps but we remove them
		when the files no longer exist (file removed in the build dir)"""

		i_existing_nodes = [x for x in parent_node.childs.values() if x.id & 3 == Node.BUILD]

		lst = set(Utils.listdir(path))
		node_names = set([x.name for x in i_existing_nodes])
		remove_names = node_names - lst

		# remove the stamps of the build nodes that no longer exist on the filesystem
		ids_to_remove = [x.id for x in i_existing_nodes if x.name in remove_names]
		cache = self.node_sigs[variant]
		for nid in ids_to_remove:
			if nid in cache:
				cache.__delitem__(nid)

	def get_env(self):
		return self.env_of_name('Release')
	def set_env(self, name, val):
		self.all_envs[name] = val

	env = property(get_env, set_env)

	def add_manual_dependency(self, path, value):
		if isinstance(path, Node.Node):
			node = path
		elif os.path.isabs(path):
			node = self.root.find_resource(path)
		else:
			node = self.path.find_resource(path)
		self.deps_man[node.id].append(value)

	def launch_node(self):
		"""return the launch directory as a node"""
		# p_ln is kind of private, but public in case if
		try:
			return self.p_ln
		except AttributeError:
			self.p_ln = self.root.find_dir(Options.launch_dir)
			return self.p_ln

	def glob(self, pattern, relative=True):
		"files matching the pattern, seen from the current folder"
		path = self.path.abspath()
		files = [self.root.find_resource(x) for x in glob.glob(path+os.sep+pattern)]
		if relative:
			files = [x.path_to_parent(self.path) for x in files if x]
		else:
			files = [x.abspath() for x in files if x]
		return files

	## the following methods are candidates for the stable apis ##

	def add_group(self, *k):
		self.task_manager.add_group(*k)

	def set_group(self, *k, **kw):
		self.task_manager.set_group(*k, **kw)

	def hash_env_vars(self, env, vars_lst):
		"""hash environment variables
		['CXX', ..] -> [env['CXX'], ..] -> md5()"""

		# ccroot objects use the same environment for building the .o at once
		# the same environment and the same variables are used

		idx = str(id(env)) + str(vars_lst)
		try: return self.cache_sig_vars[idx]
		except KeyError: pass

		lst = [str(env[a]) for a in vars_lst]
		ret = Utils.h_list(lst)
		debug('envhash: %r %r', ret, lst)

		# next time
		self.cache_sig_vars[idx] = ret
		return ret

	def name_to_obj(self, name, env):
		"""retrieve a task generator from its name or its target name
		remember that names must be unique"""
		cache = self.task_gen_cache_names
		if not cache:
			# create the index lazily
			for x in self.all_task_gen:
				vt = x.env.variant() + '_'
				if x.name:
					cache[vt + x.name] = x
				else:
					if isinstance(x.target, str):
						target = x.target
					else:
						target = ' '.join(x.target)
					v = vt + target
					if not cache.get(v, None):
						cache[v] = x
		return cache.get(env.variant() + '_' + name, None)

	def flush(self, all=1):
		"""tell the task generators to create the tasks"""

		self.ini = datetime.datetime.now()
		# force the initialization of the mapping name->object in flush
		# name_to_obj can be used in userland scripts, in that case beware of incomplete mapping
		self.task_gen_cache_names = {}
		self.name_to_obj('', self.env)

		debug('build: delayed operation TaskGen.flush() called')

		if Options.options.compile_targets:
			debug('task_gen: posting objects listed in compile_targets')

			# ensure the target names exist, fail before any post()
			target_objects = Utils.DefaultDict(list)
			for target_name in Options.options.compile_targets.split(','):
				# trim target_name (handle cases when the user added spaces to targets)
				target_name = target_name.strip()
				for env in self.all_envs.values():
					obj = self.name_to_obj(target_name, env)
					if obj:
						target_objects[target_name].append(obj)
				if not target_name in target_objects and all:
					raise Utils.WafError("target '%s' does not exist" % target_name)

			to_compile = []
			for x in target_objects.values():
				for y in x:
					to_compile.append(id(y))

			# tasks must be posted in order of declaration
			# we merely apply a filter to discard the ones we are not interested in
			for i in xrange(len(self.task_manager.groups)):
				g = self.task_manager.groups[i]
				self.task_manager.current_group = i
				if Logs.verbose:
					Logs.debug('group: group %s' % ([x for x in self.task_manager.groups_names if id(self.task_manager.groups_names[x]) == id(g)][0]))

				for tg in g.tasks_gen:
					if id(tg) in to_compile:
						if Logs.verbose:
							Logs.debug('group: %s' % tg)
						tg.post()

		else:
			debug('task_gen: posting objects (normal)')
			ln = self.launch_node()
			# if the build is started from the build directory, do as if it was started from the top-level
			# for the pretty-printing (Node.py), the two lines below cannot be moved to Build::launch_node
			if ln.is_child_of(self.bldnode) or not ln.is_child_of(self.srcnode):
				ln = self.srcnode

			# if the project file is located under the source directory, build all targets by default
			# else 'waf configure build' does nothing
			proj_node = self.root.find_dir(os.path.split(Utils.g_module.root_path)[0])
			if proj_node.id != self.srcnode.id:
				ln = self.srcnode

			for i in xrange(len(self.task_manager.groups)):
				g = self.task_manager.groups[i]
				self.task_manager.current_group = i
				if Logs.verbose:
					Logs.debug('group: group %s' % ([x for x in self.task_manager.groups_names if id(self.task_manager.groups_names[x]) == id(g)][0]))
				for tg in g.tasks_gen:
					if not tg.path.is_child_of(ln):
						continue
					if Logs.verbose:
						Logs.debug('group: %s' % tg)
					tg.post()

	def env_of_name(self, name):
		try:
			return self.all_envs[name]
		except KeyError:
			error('no such environment: '+name)
			return None

	def progress_line(self, state, total, col1, col2):
		n = len(str(total))

		Utils.rot_idx += 1
		ind = Utils.rot_chr[Utils.rot_idx % 4]

		ini = self.ini

		pc = (100.*state)/total
		eta = Utils.get_elapsed_time(ini)
		fs = "[%%%dd/%%%dd][%%s%%2d%%%%%%s][%s][" % (n, n, ind)
		left = fs % (state, total, col1, pc, col2)
		right = '][%s%s%s]' % (col1, eta, col2)

		cols = Utils.get_term_cols() - len(left) - len(right) + 2*len(col1) + 2*len(col2)
		if cols < 7: cols = 7

		ratio = int((cols*state)/total) - 1

		bar = ('='*ratio+'>').ljust(cols)
		msg = Utils.indicator % (left, bar, right)

		return msg


	# do_install is not used anywhere
	def do_install(self, src, tgt, chmod=O644):
		"""returns true if the file was effectively installed or uninstalled, false otherwise"""
		if self.is_install > 0:
			if not Options.options.force:
				# check if the file is already there to avoid a copy
				try:
					st1 = os.stat(tgt)
					st2 = os.stat(src)
				except OSError:
					pass
				else:
					# same size and identical timestamps -> make no copy
					if st1.st_mtime >= st2.st_mtime and st1.st_size == st2.st_size:
						return False

			srclbl = src.replace(self.srcnode.abspath(None)+os.sep, '')
			info("* installing %s as %s" % (srclbl, tgt))

			# following is for shared libs and stale inodes (-_-)
			try: os.remove(tgt)
			except OSError: pass

			try:
				shutil.copy2(src, tgt)
				if chmod >= 0: os.chmod(tgt, chmod)
			except IOError:
				try:
					os.stat(src)
				except (OSError, IOError):
					error('File %r does not exist' % src)
				raise Utils.WafError('Could not install the file %r' % tgt)
			return True

		elif self.is_install < 0:
			info("* uninstalling %s" % tgt)

			self.uninstall.append(tgt)

			try:
				os.remove(tgt)
			except OSError, e:
				if e.errno != errno.ENOENT:
					if not getattr(self, 'uninstall_error', None):
						self.uninstall_error = True
						Logs.warn('build: some files could not be uninstalled (retry with -vv to list them)')
					if Logs.verbose > 1:
						Logs.warn('could not remove %s (error code %r)' % (e.filename, e.errno))
			return True

	red = re.compile(r"^([A-Za-z]:)?[/\\\\]*")
	def get_install_path(self, path, env=None):
		"installation path prefixed by the destdir, the variables like in '${PREFIX}/bin' are substituted"
		if not env: env = self.env
		destdir = env.get_destdir()
		path = path.replace('/', os.sep)
		destpath = Utils.subst_vars(path, env)
		if destdir:
			destpath = os.path.join(destdir, self.red.sub('', destpath))
		return destpath

	def install_dir(self, path, env=None):
		"""
		create empty folders for the installation (very rarely used)
		"""
		if env:
			assert isinstance(env, Environment.Environment), "invalid parameter"
		else:
			env = self.env

		if not path:
			return []

		destpath = self.get_install_path(path, env)

		if self.is_install > 0:
			info('* creating %s' % destpath)
			Utils.check_dir(destpath)
		elif self.is_install < 0:
			info('* removing %s' % destpath)
			self.uninstall.append(destpath + '/xxx') # yes, ugly

	def install_files(self, path, files, env=None, chmod=O644, relative_trick=False, cwd=None):
		"""To install files only after they have been built, put the calls in a method named
		post_build on the top-level wscript

		The files must be a list and contain paths as strings or as Nodes

		The relative_trick flag can be set to install folders, use bld.path.ant_glob() with it
		"""
		if env:
			assert isinstance(env, Environment.Environment), "invalid parameter"
		else:
			env = self.env

		if not path: return []

		if not cwd:
			cwd = self.path

		if isinstance(files, str) and '*' in files:
			gl = cwd.abspath() + os.sep + files
			lst = glob.glob(gl)
		else:
			lst = Utils.to_list(files)

		if not getattr(lst, '__iter__', False):
			lst = [lst]

		destpath = self.get_install_path(path, env)

		Utils.check_dir(destpath)

		installed_files = []
		for filename in lst:
			if isinstance(filename, str) and os.path.isabs(filename):
				alst = Utils.split_path(filename)
				destfile = os.path.join(destpath, alst[-1])
			else:
				if isinstance(filename, Node.Node):
					nd = filename
				else:
					nd = cwd.find_resource(filename)
				if not nd:
					raise Utils.WafError("Unable to install the file %r (not found in %s)" % (filename, cwd))

				if relative_trick:
					destfile = os.path.join(destpath, filename)
					Utils.check_dir(os.path.dirname(destfile))
				else:
					destfile = os.path.join(destpath, nd.name)

				filename = nd.abspath(env)

			if self.do_install(filename, destfile, chmod):
				installed_files.append(destfile)
		return installed_files

	def install_as(self, path, srcfile, env=None, chmod=O644, cwd=None):
		"""
		srcfile may be a string or a Node representing the file to install

		returns True if the file was effectively installed, False otherwise
		"""
		if env:
			assert isinstance(env, Environment.Environment), "invalid parameter"
		else:
			env = self.env

		if not path:
			raise Utils.WafError("where do you want to install %r? (%r?)" % (srcfile, path))

		if not cwd:
			cwd = self.path

		destpath = self.get_install_path(path, env)

		dir, name = os.path.split(destpath)
		Utils.check_dir(dir)

		# the source path
		if isinstance(srcfile, Node.Node):
			src = srcfile.abspath(env)
		else:
			src = srcfile
			if not os.path.isabs(srcfile):
				node = cwd.find_resource(srcfile)
				if not node:
					raise Utils.WafError("Unable to install the file %r (not found in %s)" % (srcfile, cwd))
				src = node.abspath(env)

		return self.do_install(src, destpath, chmod)

	def symlink_as(self, path, src, env=None, cwd=None):
		"""example:  bld.symlink_as('${PREFIX}/lib/libfoo.so', 'libfoo.so.1.2.3') """

		if sys.platform == 'win32':
			# well, this *cannot* work
			return

		if not path:
			raise Utils.WafError("where do you want to install %r? (%r?)" % (src, path))

		tgt = self.get_install_path(path, env)

		dir, name = os.path.split(tgt)
		Utils.check_dir(dir)

		if self.is_install > 0:
			link = False
			if not os.path.islink(tgt):
				link = True
			elif os.readlink(tgt) != src:
				link = True

			if link:
				try: os.remove(tgt)
				except OSError: pass

				info('* symlink %s (-> %s)' % (tgt, src))
				os.symlink(src, tgt)
			return 0

		else: # UNINSTALL
			try:
				info('* removing %s' % (tgt))
				os.remove(tgt)
				return 0
			except OSError:
				return 1

	def exec_command(self, cmd, **kw):
		# 'runner' zone is printed out for waf -v, see wafadmin/Options.py
		debug('runner: system command -> %s', cmd)
		if self.log:
			self.log.write('%s\n' % cmd)
			kw['log'] = self.log
		try:
			if not kw.get('cwd', None):
				kw['cwd'] = self.cwd
		except AttributeError:
			self.cwd = kw['cwd'] = self.bldnode.abspath()
		return Utils.exec_command(cmd, **kw)

	def printout(self, s):
		f = self.log or sys.stderr
		f.write(s)
		f.flush()

	def add_subdirs(self, dirs):
		self.recurse(dirs, 'build')

	def pre_recurse(self, name_or_mod, path, nexdir):
		if not hasattr(self, 'oldpath'):
			self.oldpath = []
		self.oldpath.append(self.path)
		self.path = self.root.find_dir(nexdir)
		return {'bld': self, 'ctx': self}

	def post_recurse(self, name_or_mod, path, nexdir):
		self.path = self.oldpath.pop()

	###### user-defined behaviour

	def pre_build(self):
		if hasattr(self, 'pre_funs'):
			for m in self.pre_funs:
				m(self)

	def post_build(self):
		if hasattr(self, 'post_funs'):
			for m in self.post_funs:
				m(self)

	def add_pre_fun(self, meth):
		try: self.pre_funs.append(meth)
		except AttributeError: self.pre_funs = [meth]

	def add_post_fun(self, meth):
		try: self.post_funs.append(meth)
		except AttributeError: self.post_funs = [meth]

	def use_the_magic(self):
		Task.algotype = Task.MAXPARALLEL
		Task.file_deps = Task.extract_deps
		self.magic = True

	install_as = group_method(install_as)
	install_files = group_method(install_files)
	symlink_as = group_method(symlink_as)

