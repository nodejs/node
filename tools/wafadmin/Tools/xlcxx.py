#!/usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)
# Ralf Habacker, 2006 (rh)
# Yinon Ehrlich, 2009
# Michael Kuhn, 2009

import os, sys
import Configure, Options, Utils
import ccroot, ar
from Configure import conftest

@conftest
def find_xlcxx(conf):
	cxx = conf.find_program(['xlc++_r', 'xlc++'], var='CXX', mandatory=True)
	cxx = conf.cmd_to_list(cxx)
	conf.env.CXX_NAME = 'xlc++'
	conf.env.CXX      = cxx

@conftest
def find_cpp(conf):
	v = conf.env
	cpp = None
	if v['CPP']: cpp = v['CPP']
	elif 'CPP' in conf.environ: cpp = conf.environ['CPP']
	if not cpp: cpp = v['CXX']
	v['CPP'] = cpp

@conftest
def xlcxx_common_flags(conf):
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

	v['SONAME_ST']           = ''
	v['SHLIB_MARKER']        = ''
	v['STATICLIB_MARKER']    = ''
	v['FULLSTATIC_MARKER']   = '-static'

	# program
	v['program_LINKFLAGS']   = ['-Wl,-brtl']
	v['program_PATTERN']     = '%s'

	# shared library
	v['shlib_CXXFLAGS']      = ['-fPIC', '-DPIC'] # avoid using -DPIC, -fPIC aleady defines the __PIC__ macro
	v['shlib_LINKFLAGS']     = ['-G', '-Wl,-brtl,-bexpfull']
	v['shlib_PATTERN']       = 'lib%s.so'

	# static lib
	v['staticlib_LINKFLAGS'] = ''
	v['staticlib_PATTERN']   = 'lib%s.a'

def detect(conf):
	conf.find_xlcxx()
	conf.find_cpp()
	conf.find_ar()
	conf.xlcxx_common_flags()
	conf.cxx_load_tools()
	conf.cxx_add_flags()
