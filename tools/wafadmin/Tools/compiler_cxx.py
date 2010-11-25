#!/usr/bin/env python
# encoding: utf-8
# Matthias Jahn jahn dôt matthias ât freenet dôt de 2007 (pmarat)

import os, sys, imp, types, ccroot
import optparse
import Utils, Configure, Options
from Logs import debug

cxx_compiler = {
'win32':  ['g++'],
'cygwin': ['g++'],
'darwin': ['g++'],
'aix':    ['xlc++', 'g++'],
'linux':  ['g++', 'icpc', 'sunc++'],
'sunos':  ['g++', 'sunc++'],
'irix':   ['g++'],
'hpux':   ['g++'],
'default': ['g++']
}

def __list_possible_compiler(platform):
	try:
		return cxx_compiler[platform]
	except KeyError:
		return cxx_compiler["default"]

def detect(conf):
	try: test_for_compiler = Options.options.check_cxx_compiler
	except AttributeError: raise Configure.ConfigurationError("Add set_options(opt): opt.tool_options('compiler_cxx')")
	orig = conf.env
	for compiler in test_for_compiler.split():
		try:
			conf.env = orig.copy()
			conf.check_tool(compiler)
		except Configure.ConfigurationError, e:
			debug('compiler_cxx: %r' % e)
		else:
			if conf.env['CXX']:
				orig.table = conf.env.get_merged_dict()
				conf.env = orig
				conf.check_message(compiler, '', True)
				conf.env['COMPILER_CXX'] = compiler
				break
			conf.check_message(compiler, '', False)
			break
	else:
		conf.fatal('could not configure a cxx compiler!')

def set_options(opt):
	build_platform = Utils.unversioned_sys_platform()
	possible_compiler_list = __list_possible_compiler(build_platform)
	test_for_compiler = ' '.join(possible_compiler_list)
	cxx_compiler_opts = opt.add_option_group('C++ Compiler Options')
	cxx_compiler_opts.add_option('--check-cxx-compiler', default="%s" % test_for_compiler,
		help='On this platform (%s) the following C++ Compiler will be checked by default: "%s"' % (build_platform, test_for_compiler),
		dest="check_cxx_compiler")

	for cxx_compiler in test_for_compiler.split():
		opt.tool_options('%s' % cxx_compiler, option_group=cxx_compiler_opts)

