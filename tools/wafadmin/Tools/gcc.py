#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006-2008 (ita)
# Ralf Habacker, 2006 (rh)
# Yinon Ehrlich, 2009

import os, sys
import Configure, Options, Utils
import ccroot, ar
from Configure import conftest

@conftest
def find_gcc(conf):
	cc = conf.find_program(['gcc', 'cc'], var='CC', mandatory=True)
	cc = conf.cmd_to_list(cc)
	ccroot.get_cc_version(conf, cc, gcc=True)
	conf.env.CC_NAME = 'gcc'
	conf.env.CC      = cc

@conftest
def gcc_common_flags(conf):
	v = conf.env

	# CPPFLAGS CCDEFINES _CCINCFLAGS _CCDEFFLAGS

	v['CCFLAGS_DEBUG'] = ['-g']

	v['CCFLAGS_RELEASE'] = ['-O2']

	v['CC_SRC_F']            = ''
	v['CC_TGT_F']            = ['-c', '-o', ''] # shell hack for -MD
	v['CPPPATH_ST']          = '-I%s' # template for adding include paths

	# linker
	if not v['LINK_CC']: v['LINK_CC'] = v['CC']
	v['CCLNK_SRC_F']         = ''
	v['CCLNK_TGT_F']         = ['-o', ''] # shell hack for -MD

	v['LIB_ST']              = '-l%s' # template for adding libs
	v['LIBPATH_ST']          = '-L%s' # template for adding libpaths
	v['STATICLIB_ST']        = '-l%s'
	v['STATICLIBPATH_ST']    = '-L%s'
	v['RPATH_ST']            = '-Wl,-rpath,%s'
	v['CCDEFINES_ST']        = '-D%s'

	v['SONAME_ST']           = '-Wl,-h,%s'
	v['SHLIB_MARKER']        = '-Wl,-Bdynamic'
	v['STATICLIB_MARKER']    = '-Wl,-Bstatic'
	v['FULLSTATIC_MARKER']   = '-static'

	# program
	v['program_PATTERN']     = '%s'

	# shared library
	v['shlib_CCFLAGS']       = ['-fPIC', '-DPIC'] # avoid using -DPIC, -fPIC aleady defines the __PIC__ macro
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
def gcc_modifier_win32(conf):
	v = conf.env
	v['program_PATTERN']     = '%s.exe'

	v['shlib_PATTERN']       = '%s.dll'
	v['implib_PATTERN']      = 'lib%s.dll.a'
	v['IMPLIB_ST']           = '-Wl,--out-implib,%s'

	dest_arch = v['DEST_CPU']
	if dest_arch == 'x86':
		# On 32-bit x86, gcc emits a message telling the -fPIC option is ignored on this arch, so we remove that flag.
		v['shlib_CCFLAGS'] = ['-DPIC'] # TODO this is a wrong define, we don't use -fPIC!

	v.append_value('shlib_CCFLAGS', '-DDLL_EXPORT') # TODO adding nonstandard defines like this DLL_EXPORT is not a good idea

	# Auto-import is enabled by default even without this option,
	# but enabling it explicitly has the nice effect of suppressing the rather boring, debug-level messages
	# that the linker emits otherwise.
	v.append_value('LINKFLAGS', '-Wl,--enable-auto-import')

@conftest
def gcc_modifier_cygwin(conf):
	gcc_modifier_win32(conf)
	v = conf.env
	v['shlib_PATTERN']       = 'cyg%s.dll'
	v.append_value('shlib_LINKFLAGS', '-Wl,--enable-auto-image-base')

@conftest
def gcc_modifier_darwin(conf):
	v = conf.env
	v['shlib_CCFLAGS']       = ['-fPIC', '-compatibility_version', '1', '-current_version', '1']
	v['shlib_LINKFLAGS']     = ['-dynamiclib']
	v['shlib_PATTERN']       = 'lib%s.dylib'

	v['staticlib_LINKFLAGS'] = []

	v['SHLIB_MARKER']        = ''
	v['STATICLIB_MARKER']    = ''
	v['SONAME_ST']           = ''

@conftest
def gcc_modifier_aix(conf):
	v = conf.env
	v['program_LINKFLAGS']   = ['-Wl,-brtl']

	v['shlib_LINKFLAGS']     = ['-shared','-Wl,-brtl,-bexpfull']

	v['SHLIB_MARKER']        = ''

@conftest
def gcc_modifier_platform(conf):
	# * set configurations specific for a platform.
	# * the destination platform is detected automatically by looking at the macros the compiler predefines,
	#   and if it's not recognised, it fallbacks to sys.platform.
	dest_os = conf.env['DEST_OS'] or Utils.unversioned_sys_platform()
	gcc_modifier_func = globals().get('gcc_modifier_' + dest_os)
	if gcc_modifier_func:
			gcc_modifier_func(conf)

def detect(conf):
	conf.find_gcc()
	conf.find_cpp()
	conf.find_ar()
	conf.gcc_common_flags()
	conf.gcc_modifier_platform()
	conf.cc_load_tools()
	conf.cc_add_flags()
	conf.link_add_flags()

