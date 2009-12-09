#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)
# Ralf Habacker, 2006 (rh)
# Yinon Ehrlich, 2009

import os, sys
import Configure, Options, Utils
import ccroot, ar
from Configure import conftest

@conftest
def find_gxx(conf):
	cxx = conf.find_program(['g++', 'c++'], var='CXX', mandatory=True)
	cxx = conf.cmd_to_list(cxx)
	ccroot.get_cc_version(conf, cxx, gcc=True)
	conf.env.CXX_NAME = 'gcc'
	conf.env.CXX      = cxx

@conftest
def gxx_common_flags(conf):
	v = conf.env

	# CPPFLAGS CXXDEFINES _CXXINCFLAGS _CXXDEFFLAGS
	v['CXXFLAGS_DEBUG'] = ['-g']
	v['CXXFLAGS_RELEASE'] = ['-O2']

	v['CXX_SRC_F']           = ''
	v['CXX_TGT_F']           = ['-c', '-o', ''] # shell hack for -MD
	v['CPPPATH_ST']          = '-I%s' # template for adding include paths

	# linker
	if not v['LINK_CXX']: v['LINK_CXX'] = v['CXX']
	v['CXXLNK_SRC_F']        = ''
	v['CXXLNK_TGT_F']        = ['-o', ''] # shell hack for -MD

	v['LIB_ST']              = '-l%s' # template for adding libs
	v['LIBPATH_ST']          = '-L%s' # template for adding libpaths
	v['STATICLIB_ST']        = '-l%s'
	v['STATICLIBPATH_ST']    = '-L%s'
	v['RPATH_ST']            = '-Wl,-rpath,%s'
	v['CXXDEFINES_ST']       = '-D%s'

	v['SONAME_ST']           = '-Wl,-h,%s'
	v['SHLIB_MARKER']        = '-Wl,-Bdynamic'
	v['STATICLIB_MARKER']    = '-Wl,-Bstatic'
	v['FULLSTATIC_MARKER']   = '-static'

	# program
	v['program_PATTERN']     = '%s'

	# shared library
	v['shlib_CXXFLAGS']      = ['-fPIC', '-DPIC'] # avoid using -DPIC, -fPIC aleady defines the __PIC__ macro
	v['shlib_LINKFLAGS']     = ['-shared']
	v['shlib_PATTERN']       = 'lib%s.so'

	# static lib
	v['staticlib_LINKFLAGS'] = ['-Wl,-Bstatic']
	v['staticlib_PATTERN']   = 'lib%s.a'

	# osx stuff
	v['LINKFLAGS_MACBUNDLE'] = ['-bundle', '-undefined', 'dynamic_lookup']
	v['CCFLAGS_MACBUNDLE']   = ['-fPIC']
	v['macbundle_PATTERN']   = '%s.bundle'

@conftest
def gxx_modifier_win32(conf):
	v = conf.env
	v['program_PATTERN']     = '%s.exe'

	v['shlib_PATTERN']       = '%s.dll'
	v['implib_PATTERN']      = 'lib%s.dll.a'
	v['IMPLIB_ST']           = '-Wl,--out-implib,%s'

	dest_arch = v['DEST_CPU']
	if dest_arch == 'x86':
		# On 32-bit x86, gcc emits a message telling the -fPIC option is ignored on this arch, so we remove that flag.
		v['shlib_CXXFLAGS'] = ['-DPIC'] # TODO this is a wrong define, we don't use -fPIC!

	v.append_value('shlib_CXXFLAGS', '-DDLL_EXPORT') # TODO adding nonstandard defines like this DLL_EXPORT is not a good idea

	# Auto-import is enabled by default even without this option,
	# but enabling it explicitly has the nice effect of suppressing the rather boring, debug-level messages
	# that the linker emits otherwise.
	v.append_value('LINKFLAGS', '-Wl,--enable-auto-import')

@conftest
def gxx_modifier_cygwin(conf):
	gxx_modifier_win32(conf)
	v = conf.env
	v['shlib_PATTERN']       = 'cyg%s.dll'
	v.append_value('shlib_LINKFLAGS', '-Wl,--enable-auto-image-base')

@conftest
def gxx_modifier_darwin(conf):
	v = conf.env
	v['shlib_CXXFLAGS']      = ['-fPIC', '-compatibility_version', '1', '-current_version', '1']
	v['shlib_LINKFLAGS']     = ['-dynamiclib']
	v['shlib_PATTERN']       = 'lib%s.dylib'

	v['staticlib_LINKFLAGS'] = []

	v['SHLIB_MARKER']        = ''
	v['STATICLIB_MARKER']    = ''
	v['SONAME_ST']		 = ''	

@conftest
def gxx_modifier_aix(conf):
	v = conf.env
	v['program_LINKFLAGS']   = ['-Wl,-brtl']

	v['shlib_LINKFLAGS']     = ['-shared', '-Wl,-brtl,-bexpfull']

	v['SHLIB_MARKER']        = ''

@conftest
def gxx_modifier_platform(conf):
	# * set configurations specific for a platform.
	# * the destination platform is detected automatically by looking at the macros the compiler predefines,
	#   and if it's not recognised, it fallbacks to sys.platform.
	dest_os = conf.env['DEST_OS'] or Utils.unversioned_sys_platform()
	gxx_modifier_func = globals().get('gxx_modifier_' + dest_os)
	if gxx_modifier_func:
			gxx_modifier_func(conf)

def detect(conf):
	conf.find_gxx()
	conf.find_cpp()
	conf.find_ar()
	conf.gxx_common_flags()
	conf.gxx_modifier_platform()
	conf.cxx_load_tools()
	conf.cxx_add_flags()

