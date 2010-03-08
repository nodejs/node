#!/usr/bin/env python
# encoding: utf-8
# daniel.svensson at purplescout.se 2008

import os
import Task, Options, Utils
from TaskGen import before, feature, after
from Configure import conf

@feature('rubyext')
@before('apply_incpaths', 'apply_type_vars', 'apply_lib_vars', 'apply_bundle')
@after('default_cc', 'vars_target_cshlib')
def init_rubyext(self):
	self.default_install_path = '${ARCHDIR_RUBY}'
	self.uselib = self.to_list(getattr(self, 'uselib', ''))
	if not 'RUBY' in self.uselib:
		self.uselib.append('RUBY')
	if not 'RUBYEXT' in self.uselib:
		self.uselib.append('RUBYEXT')

@feature('rubyext')
@before('apply_link')
def apply_ruby_so_name(self):
	self.env['shlib_PATTERN'] = self.env['rubyext_PATTERN']

@conf
def check_ruby_version(conf, minver=()):
	"""
	Checks if ruby is installed.
	If installed the variable RUBY will be set in environment.
	Ruby binary can be overridden by --with-ruby-binary config variable
	"""

	if Options.options.rubybinary:
		conf.env.RUBY = Options.options.rubybinary
	else:
		conf.find_program("ruby", var="RUBY", mandatory=True)

	ruby = conf.env.RUBY

	try:
		version = Utils.cmd_output([ruby, '-e', 'puts defined?(VERSION) ? VERSION : RUBY_VERSION']).strip()
	except:
		conf.fatal('could not determine ruby version')
	conf.env.RUBY_VERSION = version

	try:
		ver = tuple(map(int, version.split(".")))
	except:
		conf.fatal('unsupported ruby version %r' % version)

	cver = ''
	if minver:
		if ver < minver:
			conf.fatal('ruby is too old')
		cver = ".".join(str(x) for x in minver)

	conf.check_message('ruby', cver, True, version)

@conf
def check_ruby_ext_devel(conf):
	if not conf.env.RUBY:
		conf.fatal('ruby detection is required first')

	if not conf.env.CC_NAME and not conf.env.CXX_NAME:
		conf.fatal('load a c/c++ compiler first')

	version = tuple(map(int, conf.env.RUBY_VERSION.split(".")))

	def read_out(cmd):
		return Utils.to_list(Utils.cmd_output([conf.env.RUBY, '-rrbconfig', '-e', cmd]))

	def read_config(key):
		return read_out('puts Config::CONFIG[%r]' % key)

	ruby = conf.env['RUBY']
	archdir = read_config('archdir')
	cpppath = archdir
	if version >= (1, 9, 0):
		ruby_hdrdir = read_config('rubyhdrdir')
		cpppath += ruby_hdrdir
		cpppath += [os.path.join(ruby_hdrdir[0], read_config('arch')[0])]

	conf.check(header_name='ruby.h', includes=cpppath, mandatory=True, errmsg='could not find ruby header file')

	conf.env.LIBPATH_RUBYEXT = read_config('libdir')
	conf.env.LIBPATH_RUBYEXT += archdir
	conf.env.CPPPATH_RUBYEXT = cpppath
	conf.env.CCFLAGS_RUBYEXT = read_config("CCDLFLAGS")
	conf.env.rubyext_PATTERN = '%s.' + read_config('DLEXT')[0]

	# ok this is really stupid, but the command and flags are combined.
	# so we try to find the first argument...
	flags = read_config('LDSHARED')
	while flags and flags[0][0] != '-':
		flags = flags[1:]

	# we also want to strip out the deprecated ppc flags
	if len(flags) > 1 and flags[1] == "ppc":
		flags = flags[2:]

	conf.env.LINKFLAGS_RUBYEXT = flags
	conf.env.LINKFLAGS_RUBYEXT += read_config("LIBS")
	conf.env.LINKFLAGS_RUBYEXT += read_config("LIBRUBYARG_SHARED")

	if Options.options.rubyarchdir:
		conf.env.ARCHDIR_RUBY = Options.options.rubyarchdir
	else:
		conf.env.ARCHDIR_RUBY = read_config('sitearchdir')[0]

	if Options.options.rubylibdir:
		conf.env.LIBDIR_RUBY = Options.options.rubylibdir
	else:
		conf.env.LIBDIR_RUBY = read_config('sitelibdir')[0]

def set_options(opt):
	opt.add_option('--with-ruby-archdir', type='string', dest='rubyarchdir', help='Specify directory where to install arch specific files')
	opt.add_option('--with-ruby-libdir', type='string', dest='rubylibdir', help='Specify alternate ruby library path')
	opt.add_option('--with-ruby-binary', type='string', dest='rubybinary', help='Specify alternate ruby binary')

