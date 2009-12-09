#!/usr/bin/env python
# encoding: utf-8
#
# partially based on boost.py written by Gernot Vormayr
# written by Ruediger Sonderfeld <ruediger@c-plusplus.de>, 2008
# modified by Bjoern Michaelsen, 2008
# modified by Luca Fossati, 2008
# rewritten for waf 1.5.1, Thomas Nagy, 2008
#
#def set_options(opt):
#	opt.tool_options('boost')
#	# ...
#
#def configure(conf):
#	# ... (e.g. conf.check_tool('g++'))
#	conf.check_tool('boost')
#   conf.check_boost(lib='signals filesystem', static='onlystatic', score_version=(-1000, 1000), tag_minscore=1000)
#
#def build(bld):
#   bld.new_task_gen(source='main.c', target='bar', uselib="BOOST BOOST_SYSTEM")
#
#ISSUES:
# * find_includes should be called only once!
# * support mandatory

######## boost update ###########
## ITA: * the method get_boost_version_number does work
##      * the rest of the code has not really been tried
#       * make certain a demo is provided (in demos/adv for example)

# TODO: boost.py will be removed in waf 1.6

import os.path, glob, types, re, sys
import Configure, config_c, Options, Utils, Logs
from Logs import warn
from Configure import conf

boost_code = '''
#include <iostream>
#include <boost/version.hpp>
int main() { std::cout << BOOST_VERSION << std::endl; }
'''

boost_libpath = ['/usr/lib', '/usr/local/lib', '/opt/local/lib', '/sw/lib', '/lib']
boost_cpppath = ['/usr/include', '/usr/local/include', '/opt/local/include', '/sw/include']

STATIC_NOSTATIC = 'nostatic'
STATIC_BOTH = 'both'
STATIC_ONLYSTATIC = 'onlystatic'

is_versiontag = re.compile('^\d+_\d+_?\d*$')
is_threadingtag = re.compile('^mt$')
is_abitag = re.compile('^[sgydpn]+$')
is_toolsettag = re.compile('^(acc|borland|como|cw|dmc|darwin|gcc|hp_cxx|intel|kylix|vc|mgw|qcc|sun|vacpp)\d*$')

def set_options(opt):
	opt.add_option('--boost-includes', type='string', default='', dest='boostincludes', help='path to the boost directory where the includes are e.g. /usr/local/include/boost-1_35')
	opt.add_option('--boost-libs', type='string', default='', dest='boostlibs', help='path to the directory where the boost libs are e.g. /usr/local/lib')

def string_to_version(s):
	version = s.split('.')
	if len(version) < 3: return 0
	return int(version[0])*100000 + int(version[1])*100 + int(version[2])

def version_string(version):
	major = version / 100000
	minor = version / 100 % 1000
	minor_minor = version % 100
	if minor_minor == 0:
		return "%d_%d" % (major, minor)
	else:
		return "%d_%d_%d" % (major, minor, minor_minor)

def libfiles(lib, pattern, lib_paths):
	result = []
	for lib_path in lib_paths:
		libname = pattern % ('boost_' + lib + '*')
		result += glob.glob(lib_path + '/' + libname)
	return result

@conf
def get_boost_version_number(self, dir):
	"""silently retrieve the boost version number"""
	try:
		return self.run_c_code(compiler='cxx', code=boost_code, includes=dir, execute=1, env=self.env.copy(), type='cprogram', compile_mode='cxx', compile_filename='test.cpp')
	except Configure.ConfigurationError, e:
		return -1

def set_default(kw, var, val):
	if not var in kw:
		kw[var] = val

def tags_score(tags, kw):
	"""
	checks library tags

	see http://www.boost.org/doc/libs/1_35_0/more/getting_started/unix-variants.html 6.1
	"""
	score = 0
	needed_tags = {
		'threading': kw['tag_threading'],
		'abi': kw['tag_abi'],
		'toolset': kw['tag_toolset'],
		'version': kw['tag_version']
	}

	if kw['tag_toolset'] is None:
		v = kw['env']
		toolset = v['CXX_NAME']
		if v['CXX_VERSION']:
			version_no = v['CXX_VERSION'].split('.')
			toolset += version_no[0]
			if len(version_no) > 1:
				toolset += version_no[1]
		needed_tags['toolset'] = toolset

	found_tags = {}
	for tag in tags:
		if is_versiontag.match(tag): found_tags['version'] = tag
		if is_threadingtag.match(tag): found_tags['threading'] = tag
		if is_abitag.match(tag): found_tags['abi'] = tag
		if is_toolsettag.match(tag): found_tags['toolset'] = tag

	for tagname in needed_tags.iterkeys():
		if needed_tags[tagname] is not None and tagname in found_tags:
			if re.compile(needed_tags[tagname]).match(found_tags[tagname]):
				score += kw['score_' + tagname][0]
			else:
				score += kw['score_' + tagname][1]
	return score

@conf
def validate_boost(self, kw):
	ver = kw.get('version', '')

	for x in 'min_version max_version version'.split():
		set_default(kw, x, ver)

	set_default(kw, 'lib', '')
	kw['lib'] = Utils.to_list(kw['lib'])

	set_default(kw, 'env', self.env)

	set_default(kw, 'libpath', boost_libpath)
	set_default(kw, 'cpppath', boost_cpppath)

	for x in 'tag_threading tag_version tag_toolset'.split():
		set_default(kw, x, None)
	set_default(kw, 'tag_abi', '^[^d]*$')

	set_default(kw, 'score_threading', (10, -10))
	set_default(kw, 'score_abi', (10, -10))
	set_default(kw, 'score_toolset', (1, -1))
	set_default(kw, 'score_version', (100, -100))

	set_default(kw, 'score_min', 0)
	set_default(kw, 'static', STATIC_NOSTATIC)
	set_default(kw, 'found_includes', False)
	set_default(kw, 'min_score', 0)

	set_default(kw, 'errmsg', 'not found')
	set_default(kw, 'okmsg', 'ok')

@conf
def find_boost_includes(self, kw):
	"""
	check every path in kw['cpppath'] for subdir
	that either starts with boost- or is named boost.

	Then the version is checked and selected accordingly to
	min_version/max_version. The highest possible version number is
	selected!

	If no versiontag is set the versiontag is set accordingly to the
	selected library and CPPPATH_BOOST is set.
	"""
	boostPath = getattr(Options.options, 'boostincludes', '')
	if boostPath:
		boostPath = [os.path.normpath(os.path.expandvars(os.path.expanduser(boostPath)))]
	else:
		boostPath = Utils.to_list(kw['cpppath'])

	min_version = string_to_version(kw.get('min_version', ''))
	max_version = string_to_version(kw.get('max_version', '')) or (sys.maxint - 1)

	version = 0
	for include_path in boostPath:
		boost_paths = glob.glob(os.path.join(include_path, 'boost*'))
		for path in boost_paths:
			pathname = os.path.split(path)[-1]
			ret = -1
			if pathname == 'boost':
				path = include_path
				ret = self.get_boost_version_number(path)
			elif pathname.startswith('boost-'):
				ret = self.get_boost_version_number(path)
			ret = int(ret)

			if ret != -1 and ret >= min_version and ret <= max_version and ret > version:
				boost_path = path
				version = ret
	if not version:
		self.fatal('boost headers not found! (required version min: %s max: %s)'
			  % (kw['min_version'], kw['max_version']))
		return False

	found_version = version_string(version)
	versiontag = '^' + found_version + '$'
	if kw['tag_version'] is None:
		kw['tag_version'] = versiontag
	elif kw['tag_version'] != versiontag:
		warn('boost header version %r and tag_version %r do not match!' % (versiontag, kw['tag_version']))
	env = self.env
	env['CPPPATH_BOOST'] = boost_path
	env['BOOST_VERSION'] = found_version
	self.found_includes = 1
	ret = 'Version %s (%s)' % (found_version, boost_path)
	return ret

@conf
def find_boost_library(self, lib, kw):

	def find_library_from_list(lib, files):
		lib_pattern = re.compile('.*boost_(.*?)\..*')
		result = (None, None)
		resultscore = kw['min_score'] - 1
		for file in files:
			m = lib_pattern.search(file, 1)
			if m:
				libname = m.group(1)
				libtags = libname.split('-')[1:]
				currentscore = tags_score(libtags, kw)
				if currentscore > resultscore:
					result = (libname, file)
					resultscore = currentscore
		return result

	lib_paths = getattr(Options.options, 'boostlibs', '')
	if lib_paths:
		lib_paths = [os.path.normpath(os.path.expandvars(os.path.expanduser(lib_paths)))]
	else:
		lib_paths = Utils.to_list(kw['libpath'])

	v = kw.get('env', self.env)

	(libname, file) = (None, None)
	if kw['static'] in [STATIC_NOSTATIC, STATIC_BOTH]:
		st_env_prefix = 'LIB'
		files = libfiles(lib, v['shlib_PATTERN'], lib_paths)
		(libname, file) = find_library_from_list(lib, files)
	if libname is None and kw['static'] in [STATIC_ONLYSTATIC, STATIC_BOTH]:
		st_env_prefix = 'STATICLIB'
		staticLibPattern = v['staticlib_PATTERN']
		if self.env['CC_NAME'] == 'msvc':
			staticLibPattern = 'lib' + staticLibPattern
		files = libfiles(lib, staticLibPattern, lib_paths)
		(libname, file) = find_library_from_list(lib, files)
	if libname is not None:
		v['LIBPATH_BOOST_' + lib.upper()] = [os.path.split(file)[0]]
		if self.env['CC_NAME'] == 'msvc' and os.path.splitext(file)[1] == '.lib':
			v[st_env_prefix + '_BOOST_' + lib.upper()] = ['libboost_'+libname]
		else:
			v[st_env_prefix + '_BOOST_' + lib.upper()] = ['boost_'+libname]
		return
	self.fatal('lib boost_' + lib + ' not found!')

@conf
def check_boost(self, *k, **kw):
	"""
	This should be the main entry point

- min_version
- max_version
- version
- include_path
- lib_path
- lib
- toolsettag   - None or a regexp
- threadingtag - None or a regexp
- abitag       - None or a regexp
- versiontag   - WARNING: you should rather use version or min_version/max_version
- static       - look for static libs (values:
	  'nostatic'   or STATIC_NOSTATIC   - ignore static libs (default)
	  'both'       or STATIC_BOTH       - find static libs, too
	  'onlystatic' or STATIC_ONLYSTATIC - find only static libs
- score_version
- score_abi
- scores_threading
- score_toolset
 * the scores are tuples (match_score, nomatch_score)
   match_score is the added to the score if the tag is matched
   nomatch_score is added when a tag is found and does not match
- min_score
	"""

	if not self.env['CXX']:
		self.fatal('load a c++ compiler tool first, for example conf.check_tool("g++")')
	self.validate_boost(kw)
	ret = None
	try:
		if not kw.get('found_includes', None):
			self.check_message_1(kw.get('msg_includes', 'boost headers'))
			ret = self.find_boost_includes(kw)

	except Configure.ConfigurationError, e:
		if 'errmsg' in kw:
			self.check_message_2(kw['errmsg'], 'YELLOW')
		if 'mandatory' in kw:
			if Logs.verbose > 1:
				raise
			else:
				self.fatal('the configuration failed (see %r)' % self.log.name)
	else:
		if 'okmsg' in kw:
			self.check_message_2(kw.get('okmsg_includes', ret))

	for lib in kw['lib']:
		self.check_message_1('library boost_'+lib)
		try:
			self.find_boost_library(lib, kw)
		except Configure.ConfigurationError, e:
			ret = False
			if 'errmsg' in kw:
				self.check_message_2(kw['errmsg'], 'YELLOW')
			if 'mandatory' in kw:
				if Logs.verbose > 1:
					raise
				else:
					self.fatal('the configuration failed (see %r)' % self.log.name)
		else:
			if 'okmsg' in kw:
				self.check_message_2(kw['okmsg'])

	return ret

