import os
import TaskGen, Utils, Runner, Options, Build
from TaskGen import extension, taskgen, before, after, feature
from Configure import conf, conftest

@taskgen
@before('apply_incpaths', 'apply_lib_vars', 'apply_type_vars')
@feature('node_addon')
@before('apply_bundle')
def init_node_addon(self):
	self.default_install_path = self.env['NODE_PATH']
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

  conf.env['PREFIX_NODE'] = get_prefix()
  prefix = conf.env['PREFIX_NODE']
  lib = join(prefix, 'lib')
  nodebin = join(prefix, 'bin', 'node')

  conf.env['LIBPATH_NODE'] = lib
  conf.env['CPPPATH_NODE'] = join(prefix, 'include', 'node')

  conf.env.append_value('CPPFLAGS_NODE', '-D_GNU_SOURCE')

  conf.env.append_value('CCFLAGS_NODE', '-D_LARGEFILE_SOURCE')
  conf.env.append_value('CCFLAGS_NODE', '-D_FILE_OFFSET_BITS=64')

  conf.env.append_value('CXXFLAGS_NODE', '-D_LARGEFILE_SOURCE')
  conf.env.append_value('CXXFLAGS_NODE', '-D_FILE_OFFSET_BITS=64')

  # with symbols
  conf.env.append_value('CCFLAGS', ['-g'])
  conf.env.append_value('CXXFLAGS', ['-g'])
  # install path
  conf.env['NODE_PATH'] = get_node_path()
  # this changes the install path of cxx task_gen
  conf.env['LIBDIR'] = conf.env['NODE_PATH']

  found = os.path.exists(conf.env['NODE_PATH'])
  conf.check_message('node path', '', found, conf.env['NODE_PATH'])

  found = os.path.exists(nodebin)
  conf.check_message('node prefix', '', found, prefix)

  ## On Cygwin we need to link to the generated symbol definitions
  if Options.platform.startswith('cygwin'): conf.env['LIB_NODE'] = 'node'

  ## On Mac OSX we need to use mac bundles
  if Options.platform == 'darwin':
    if 'i386' in Utils.cmd_output(['file', nodebin]):
      conf.env.append_value('CPPFLAGS_NODE', ['-arch', 'i386'])
      conf.env.append_value('CXXFLAGS_NODE', ['-arch', 'i386'])
      conf.env.append_value('LINKFLAGS', ['-arch', 'i386'])
      conf.env['DEST_CPU'] = 'i386'
    conf.check_tool('osx')

def get_node_path():
    join = os.path.join
    nodePath = None
    if not os.environ.has_key('NODE_PATH'):
        if not os.environ.has_key('HOME'):
            nodePath = join(get_prefix(), 'lib', 'node')
        else:
            nodePath = join(os.environ['HOME'], '.node_libraries')
    else:
        nodePath = os.environ['NODE_PATH']
    return nodePath

def get_prefix():
    prefix = None
    if not os.environ.has_key('PREFIX_NODE'):
        prefix = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
    else:
        prefix = os.environ['PREFIX_NODE']
    return prefix
