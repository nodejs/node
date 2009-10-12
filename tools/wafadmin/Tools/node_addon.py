import os
import TaskGen, Utils, Utils, Runner, Options, Build
from TaskGen import extension, taskgen, before, after, feature
from Configure import conf, conftest

@taskgen
@before('apply_incpaths', 'apply_lib_vars', 'apply_type_vars')
@feature('node_addon')
@before('apply_bundle')
def init_node_addon(self):
	self.default_install_path = '${PREFIX_NODE}/lib/node/libraries'
	self.uselib = self.to_list(getattr(self, 'uselib', ''))
	if not 'NODE' in self.uselib: self.uselib.append('NODE')
	self.env['MACBUNDLE'] = True

@taskgen
@before('apply_link', 'apply_lib_vars', 'apply_type_vars')
@after('apply_bundle')
@feature('node_addon')
def node_addon_shlib_ext(self):
	self.env['shlib_PATTERN'] = "%s.node"

def detect(conf):
  join = os.path.join
  abspath = os.path.abspath
  wafadmin = abspath(join(os.path.dirname(__file__), '..'))
  libnode = abspath(join(wafadmin, '..'))
  lib = abspath(join(libnode, '..'))
  prefix = abspath(join(lib, '..'))

  conf.env['PREFIX_NODE'] = prefix
  conf.env['LIBPATH_NODE'] = lib
  conf.env['CPPPATH_NODE'] = join(prefix, 'include/node')
  conf.env['CPPFLAGS_NODE'] = '-D_GNU_SOURCE'
  conf.env['CPPFLAGS_NODE'] = '-DEV_MULTIPLICITY=0'

  found = os.path.exists(join(prefix, "bin/node"))
  conf.check_message('node prefix', '', found, prefix)

  ## On Mac OSX we need to use mac bundles
  if Options.platform == 'darwin': conf.check_tool('osx')

