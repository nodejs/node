#!/usr/bin/python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gyp
import gyp.common
import gyp.SCons as SCons
import os.path
import pprint
import re


# TODO:  remove when we delete the last WriteList() call in this module
WriteList = SCons.WriteList


generator_default_variables = {
    'EXECUTABLE_PREFIX': '',
    'EXECUTABLE_SUFFIX': '',
    'STATIC_LIB_PREFIX': '${LIBPREFIX}',
    'SHARED_LIB_PREFIX': '${SHLIBPREFIX}',
    'STATIC_LIB_SUFFIX': '${LIBSUFFIX}',
    'SHARED_LIB_SUFFIX': '${SHLIBSUFFIX}',
    'INTERMEDIATE_DIR': '${INTERMEDIATE_DIR}',
    'SHARED_INTERMEDIATE_DIR': '${SHARED_INTERMEDIATE_DIR}',
    'OS': 'linux',
    'PRODUCT_DIR': '$TOP_BUILDDIR',
    'SHARED_LIB_DIR': '$LIB_DIR',
    'LIB_DIR': '$LIB_DIR',
    'RULE_INPUT_ROOT': '${SOURCE.filebase}',
    'RULE_INPUT_DIRNAME': '${SOURCE.dir}',
    'RULE_INPUT_EXT': '${SOURCE.suffix}',
    'RULE_INPUT_NAME': '${SOURCE.file}',
    'RULE_INPUT_PATH': '${SOURCE.abspath}',
    'CONFIGURATION_NAME': '${CONFIG_NAME}',
}

# Tell GYP how to process the input for us.
generator_handles_variants = True
generator_wants_absolute_build_file_paths = True


def FixPath(path, prefix):
  if not os.path.isabs(path) and not path[0] == '$':
    path = prefix + path
  return path


header = """\
# This file is generated; do not edit.
"""


_alias_template = """
if GetOption('verbose'):
  _action = Action([%(action)s])
else:
  _action = Action([%(action)s], %(message)s)
_outputs = env.Alias(
  ['_%(target_name)s_action'],
  %(inputs)s,
  _action
)
env.AlwaysBuild(_outputs)
"""

_run_as_template = """
if GetOption('verbose'):
  _action = Action([%(action)s])
else:
  _action = Action([%(action)s], %(message)s)
"""

_run_as_template_suffix = """
_run_as_target = env.Alias('run_%(target_name)s', target_files, _action)
env.Requires(_run_as_target, [
    Alias('%(target_name)s'),
])
env.AlwaysBuild(_run_as_target)
"""

_command_template = """
if GetOption('verbose'):
  _action = Action([%(action)s])
else:
  _action = Action([%(action)s], %(message)s)
_outputs = env.Command(
  %(outputs)s,
  %(inputs)s,
  _action
)
"""

# This is copied from the default SCons action, updated to handle symlinks.
_copy_action_template = """
import shutil
import SCons.Action

def _copy_files_or_dirs_or_symlinks(dest, src):
  SCons.Node.FS.invalidate_node_memos(dest)
  if SCons.Util.is_List(src) and os.path.isdir(dest):
    for file in src:
      shutil.copy2(file, dest)
    return 0
  elif os.path.islink(src):
    linkto = os.readlink(src)
    os.symlink(linkto, dest)
    return 0
  elif os.path.isfile(src):
    return shutil.copy2(src, dest)
  else:
    return shutil.copytree(src, dest, 1)

def _copy_files_or_dirs_or_symlinks_str(dest, src):
  return 'Copying %s to %s ...' % (src, dest)

GYPCopy = SCons.Action.ActionFactory(_copy_files_or_dirs_or_symlinks,
                                     _copy_files_or_dirs_or_symlinks_str,
                                     convert=str)
"""

_rule_template = """
%(name)s_additional_inputs = %(inputs)s
%(name)s_outputs = %(outputs)s
def %(name)s_emitter(target, source, env):
  return (%(name)s_outputs, source + %(name)s_additional_inputs)
if GetOption('verbose'):
  %(name)s_action = Action([%(action)s])
else:
  %(name)s_action = Action([%(action)s], %(message)s)
env['BUILDERS']['%(name)s'] = Builder(action=%(name)s_action,
                                      emitter=%(name)s_emitter)

_outputs = []
_processed_input_files = []
for infile in input_files:
  if (type(infile) == type('')
      and not os.path.isabs(infile)
      and not infile[0] == '$'):
    infile = %(src_dir)r + infile
  if str(infile).endswith('.%(extension)s'):
    _generated = env.%(name)s(infile)
    env.Precious(_generated)
    _outputs.append(_generated)
    %(process_outputs_as_sources_line)s
  else:
    _processed_input_files.append(infile)
prerequisites.extend(_outputs)
input_files = _processed_input_files
"""

_spawn_hack = """
import re
import SCons.Platform.posix
needs_shell = re.compile('["\\'><!^&]')
def gyp_spawn(sh, escape, cmd, args, env):
  def strip_scons_quotes(arg):
    if arg[0] == '"' and arg[-1] == '"':
      return arg[1:-1]
    return arg
  stripped_args = [strip_scons_quotes(a) for a in args]
  if needs_shell.search(' '.join(stripped_args)):
    return SCons.Platform.posix.exec_spawnvpe([sh, '-c', ' '.join(args)], env)
  else:
    return SCons.Platform.posix.exec_spawnvpe(stripped_args, env)
"""


def EscapeShellArgument(s):
  """Quotes an argument so that it will be interpreted literally by a POSIX
     shell. Taken from
     http://stackoverflow.com/questions/35817/whats-the-best-way-to-escape-ossystem-calls-in-python
     """
  return "'" + s.replace("'", "'\\''") + "'"


def InvertNaiveSConsQuoting(s):
  """SCons tries to "help" with quoting by naively putting double-quotes around
     command-line arguments containing space or tab, which is broken for all
     but trivial cases, so we undo it. (See quote_spaces() in Subst.py)"""
  if ' ' in s or '\t' in s:
    # Then SCons will put double-quotes around this, so add our own quotes
    # to close its quotes at the beginning and end.
    s = '"' + s + '"'
  return s


def EscapeSConsVariableExpansion(s):
  """SCons has its own variable expansion syntax using $. We must escape it for
    strings to be interpreted literally. For some reason this requires four
    dollar signs, not two, even without the shell involved."""
  return s.replace('$', '$$$$')


def EscapeCppDefine(s):
  """Escapes a CPP define so that it will reach the compiler unaltered."""
  s = EscapeShellArgument(s)
  s = InvertNaiveSConsQuoting(s)
  s = EscapeSConsVariableExpansion(s)
  return s


def GenerateConfig(fp, config, indent='', src_dir=''):
  """
  Generates SCons dictionary items for a gyp configuration.

  This provides the main translation between the (lower-case) gyp settings
  keywords and the (upper-case) SCons construction variables.
  """
  var_mapping = {
      'ASFLAGS' : 'asflags',
      'CCFLAGS' : 'cflags',
      'CFLAGS' : 'cflags_c',
      'CXXFLAGS' : 'cflags_cc',
      'CPPDEFINES' : 'defines',
      'CPPPATH' : 'include_dirs',
      # Add the ldflags value to $LINKFLAGS, but not $SHLINKFLAGS.
      # SCons defines $SHLINKFLAGS to incorporate $LINKFLAGS, so
      # listing both here would case 'ldflags' to get appended to
      # both, and then have it show up twice on the command line.
      'LINKFLAGS' : 'ldflags',
  }
  postamble='\n%s],\n' % indent
  for scons_var in sorted(var_mapping.keys()):
      gyp_var = var_mapping[scons_var]
      value = config.get(gyp_var)
      if value:
        if gyp_var in ('defines',):
          value = [EscapeCppDefine(v) for v in value]
        if gyp_var in ('include_dirs',):
          if src_dir and not src_dir.endswith('/'):
            src_dir += '/'
          result = []
          for v in value:
            v = FixPath(v, src_dir)
            # Force SCons to evaluate the CPPPATH directories at
            # SConscript-read time, so delayed evaluation of $SRC_DIR
            # doesn't point it to the --generator-output= directory.
            result.append('env.Dir(%r)' % v)
          value = result
        else:
          value = map(repr, value)
        WriteList(fp,
                  value,
                  prefix=indent,
                  preamble='%s%s = [\n    ' % (indent, scons_var),
                  postamble=postamble)


def GenerateSConscript(output_filename, spec, build_file, build_file_data):
  """
  Generates a SConscript file for a specific target.

  This generates a SConscript file suitable for building any or all of
  the target's configurations.

  A SConscript file may be called multiple times to generate targets for
  multiple configurations.  Consequently, it needs to be ready to build
  the target for any requested configuration, and therefore contains
  information about the settings for all configurations (generated into
  the SConscript file at gyp configuration time) as well as logic for
  selecting (at SCons build time) the specific configuration being built.

  The general outline of a generated SConscript file is:

    --  Header

    --  Import 'env'.  This contains a $CONFIG_NAME construction
        variable that specifies what configuration to build
        (e.g. Debug, Release).

    --  Configurations.  This is a dictionary with settings for
        the different configurations (Debug, Release) under which this
        target can be built.  The values in the dictionary are themselves
        dictionaries specifying what construction variables should added
        to the local copy of the imported construction environment
        (Append), should be removed (FilterOut), and should outright
        replace the imported values (Replace).

    --  Clone the imported construction environment and update
        with the proper configuration settings.

    --  Initialize the lists of the targets' input files and prerequisites.

    --  Target-specific actions and rules.  These come after the
        input file and prerequisite initializations because the
        outputs of the actions and rules may affect the input file
        list (process_outputs_as_sources) and get added to the list of
        prerequisites (so that they're guaranteed to be executed before
        building the target).

    --  Call the Builder for the target itself.

    --  Arrange for any copies to be made into installation directories.

    --  Set up the {name} Alias (phony Node) for the target as the
        primary handle for building all of the target's pieces.

    --  Use env.Require() to make sure the prerequisites (explicitly
        specified, but also including the actions and rules) are built
        before the target itself.

    --  Return the {name} Alias to the calling SConstruct file
        so it can be added to the list of default targets.
  """
  scons_target = SCons.Target(spec)

  gyp_dir = os.path.dirname(output_filename)
  if not gyp_dir:
      gyp_dir = '.'
  gyp_dir = os.path.abspath(gyp_dir)

  output_dir = os.path.dirname(output_filename)
  src_dir = build_file_data['_DEPTH']
  src_dir_rel = gyp.common.RelativePath(src_dir, output_dir)
  subdir = gyp.common.RelativePath(os.path.dirname(build_file), src_dir)
  src_subdir = '$SRC_DIR/' + subdir
  src_subdir_ = src_subdir + '/'

  component_name = os.path.splitext(os.path.basename(build_file))[0]
  target_name = spec['target_name']

  if not os.path.exists(gyp_dir):
    os.makedirs(gyp_dir)
  fp = open(output_filename, 'w')
  fp.write(header)

  fp.write('\nimport os\n')
  fp.write('\nImport("env")\n')

  #
  fp.write('\n')
  fp.write('env = env.Clone(COMPONENT_NAME=%s,\n' % repr(component_name))
  fp.write('                TARGET_NAME=%s)\n' % repr(target_name))

  #
  for config in spec['configurations'].itervalues():
    if config.get('scons_line_length'):
      fp.write(_spawn_hack)
      break

  #
  indent = ' ' * 12
  fp.write('\n')
  fp.write('configurations = {\n')
  for config_name, config in spec['configurations'].iteritems():
    fp.write('    \'%s\' : {\n' % config_name)

    fp.write('        \'Append\' : dict(\n')
    GenerateConfig(fp, config, indent, src_subdir)
    libraries = spec.get('libraries')
    if libraries:
      WriteList(fp,
                map(repr, libraries),
                prefix=indent,
                preamble='%sLIBS = [\n    ' % indent,
                postamble='\n%s],\n' % indent)
    fp.write('        ),\n')

    fp.write('        \'FilterOut\' : dict(\n' )
    for key, var in config.get('scons_remove', {}).iteritems():
      fp.write('             %s = %s,\n' % (key, repr(var)))
    fp.write('        ),\n')

    fp.write('        \'Replace\' : dict(\n' )
    scons_settings = config.get('scons_variable_settings', {})
    for key in sorted(scons_settings.keys()):
      val = pprint.pformat(scons_settings[key])
      fp.write('             %s = %s,\n' % (key, val))
    if 'c++' in spec.get('link_languages', []):
      fp.write('             %s = %s,\n' % ('LINK', repr('$CXX')))
    if config.get('scons_line_length'):
      fp.write('             SPAWN = gyp_spawn,\n')
    fp.write('        ),\n')

    fp.write('        \'ImportExternal\' : [\n' )
    for var in config.get('scons_import_variables', []):
      fp.write('             %s,\n' % repr(var))
    fp.write('        ],\n')

    fp.write('        \'PropagateExternal\' : [\n' )
    for var in config.get('scons_propagate_variables', []):
      fp.write('             %s,\n' % repr(var))
    fp.write('        ],\n')

    fp.write('    },\n')
  fp.write('}\n')

  fp.write('\n'
           'config = configurations[env[\'CONFIG_NAME\']]\n'
           'env.Append(**config[\'Append\'])\n'
           'env.FilterOut(**config[\'FilterOut\'])\n'
           'env.Replace(**config[\'Replace\'])\n')

  fp.write('\n'
           '# Scons forces -fPIC for SHCCFLAGS on some platforms.\n'
           '# Disable that so we can control it from cflags in gyp.\n'
           '# Note that Scons itself is inconsistent with its -fPIC\n'
           '# setting. SHCCFLAGS forces -fPIC, and SHCFLAGS does not.\n'
           '# This will make SHCCFLAGS consistent with SHCFLAGS.\n'
           'env[\'SHCCFLAGS\'] = [\'$CCFLAGS\']\n')

  fp.write('\n'
           'for _var in config[\'ImportExternal\']:\n'
           '  if _var in ARGUMENTS:\n'
           '    env[_var] = ARGUMENTS[_var]\n'
           '  elif _var in os.environ:\n'
           '    env[_var] = os.environ[_var]\n'
           'for _var in config[\'PropagateExternal\']:\n'
           '  if _var in ARGUMENTS:\n'
           '    env[_var] = ARGUMENTS[_var]\n'
           '  elif _var in os.environ:\n'
           '    env[\'ENV\'][_var] = os.environ[_var]\n')

  fp.write('\n'
           "env['ENV']['LD_LIBRARY_PATH'] = env.subst('$LIB_DIR')\n")

  #
  #fp.write("\nif env.has_key('CPPPATH'):\n")
  #fp.write("  env['CPPPATH'] = map(env.Dir, env['CPPPATH'])\n")

  variants = spec.get('variants', {})
  for setting in sorted(variants.keys()):
    if_fmt = 'if ARGUMENTS.get(%s) not in (None, \'0\'):\n'
    fp.write('\n')
    fp.write(if_fmt % repr(setting.upper()))
    fp.write('  env.AppendUnique(\n')
    GenerateConfig(fp, variants[setting], indent, src_subdir)
    fp.write('  )\n')

  #
  scons_target.write_input_files(fp)

  fp.write('\n')
  fp.write('target_files = []\n')
  prerequisites = spec.get('scons_prerequisites', [])
  fp.write('prerequisites = %s\n' % pprint.pformat(prerequisites))

  actions = spec.get('actions', [])
  for action in actions:
    a = ['cd', src_subdir, '&&'] + action['action']
    message = action.get('message')
    if message:
      message = repr(message)
    inputs = [FixPath(f, src_subdir_) for f in action.get('inputs', [])]
    outputs = [FixPath(f, src_subdir_) for f in action.get('outputs', [])]
    if outputs:
      template = _command_template
    else:
      template = _alias_template
    fp.write(template % {
                 'inputs' : pprint.pformat(inputs),
                 'outputs' : pprint.pformat(outputs),
                 'action' : pprint.pformat(a),
                 'message' : message,
                 'target_name': target_name,
             })
    if int(action.get('process_outputs_as_sources', 0)):
      fp.write('input_files.extend(_outputs)\n')
    fp.write('prerequisites.extend(_outputs)\n')
    fp.write('target_files.extend(_outputs)\n')

  rules = spec.get('rules', [])
  for rule in rules:
    name = rule['rule_name']
    a = ['cd', src_subdir, '&&'] + rule['action']
    message = rule.get('message')
    if message:
        message = repr(message)
    if int(rule.get('process_outputs_as_sources', 0)):
      poas_line = '_processed_input_files.extend(_generated)'
    else:
      poas_line = '_processed_input_files.append(infile)'
    inputs = [FixPath(f, src_subdir_) for f in rule.get('inputs', [])]
    outputs = [FixPath(f, src_subdir_) for f in rule.get('outputs', [])]
    fp.write(_rule_template % {
                 'inputs' : pprint.pformat(inputs),
                 'outputs' : pprint.pformat(outputs),
                 'action' : pprint.pformat(a),
                 'extension' : rule['extension'],
                 'name' : name,
                 'message' : message,
                 'process_outputs_as_sources_line' : poas_line,
                 'src_dir' : src_subdir_,
             })

  scons_target.write_target(fp, src_subdir)

  copies = spec.get('copies', [])
  if copies:
    fp.write(_copy_action_template)
  for copy in copies:
    destdir = None
    files = None
    try:
      destdir = copy['destination']
    except KeyError, e:
      gyp.common.ExceptionAppend(
        e,
        "Required 'destination' key missing for 'copies' in %s." % build_file)
      raise
    try:
      files = copy['files']
    except KeyError, e:
      gyp.common.ExceptionAppend(
        e, "Required 'files' key missing for 'copies' in %s." % build_file)
      raise
    if not files:
      # TODO:  should probably add a (suppressible) warning;
      # a null file list may be unintentional.
      continue
    if not destdir:
      raise Exception(
        "Required 'destination' key is empty for 'copies' in %s." % build_file)

    fmt = ('\n'
           '_outputs = env.Command(%s,\n'
           '    %s,\n'
           '    GYPCopy(\'$TARGET\', \'$SOURCE\'))\n')
    for f in copy['files']:
      # Remove trailing separators so basename() acts like Unix basename and
      # always returns the last element, whether a file or dir. Without this,
      # only the contents, not the directory itself, are copied (and nothing
      # might be copied if dest already exists, since scons thinks nothing needs
      # to be done).
      dest = os.path.join(destdir, os.path.basename(f.rstrip(os.sep)))
      f = FixPath(f, src_subdir_)
      dest = FixPath(dest, src_subdir_)
      fp.write(fmt % (repr(dest), repr(f)))
      fp.write('target_files.extend(_outputs)\n')

  run_as = spec.get('run_as')
  if run_as:
    action = run_as.get('action', [])
    working_directory = run_as.get('working_directory')
    if not working_directory:
      working_directory = gyp_dir
    else:
      if not os.path.isabs(working_directory):
        working_directory = os.path.normpath(os.path.join(gyp_dir,
                                                          working_directory))
    if run_as.get('environment'):
      for (key, val) in run_as.get('environment').iteritems():
        action = ['%s="%s"' % (key, val)] + action
    action = ['cd', '"%s"' % working_directory, '&&'] + action
    fp.write(_run_as_template % {
      'action' : pprint.pformat(action),
      'message' : run_as.get('message', ''),
    })

  fmt = "\ngyp_target = env.Alias('%s', target_files)\n"
  fp.write(fmt % target_name)

  dependencies = spec.get('scons_dependencies', [])
  if dependencies:
    WriteList(fp, dependencies, preamble='dependencies = [\n    ',
                                postamble='\n]\n')
    fp.write('env.Requires(target_files, dependencies)\n')
    fp.write('env.Requires(gyp_target, dependencies)\n')
    fp.write('for prerequisite in prerequisites:\n')
    fp.write('  env.Requires(prerequisite, dependencies)\n')
  fp.write('env.Requires(gyp_target, prerequisites)\n')

  if run_as:
    fp.write(_run_as_template_suffix % {
      'target_name': target_name,
    })

  fp.write('Return("gyp_target")\n')

  fp.close()


#############################################################################
# TEMPLATE BEGIN

_wrapper_template = """\

__doc__ = '''
Wrapper configuration for building this entire "solution,"
including all the specific targets in various *.scons files.
'''

import os
import sys

import SCons.Environment
import SCons.Util

def GetProcessorCount():
  '''
  Detects the number of CPUs on the system. Adapted form:
  http://codeliberates.blogspot.com/2008/05/detecting-cpuscores-in-python.html
  '''
  # Linux, Unix and Mac OS X:
  if hasattr(os, 'sysconf'):
    if os.sysconf_names.has_key('SC_NPROCESSORS_ONLN'):
      # Linux and Unix or Mac OS X with python >= 2.5:
      return os.sysconf('SC_NPROCESSORS_ONLN')
    else:  # Mac OS X with Python < 2.5:
      return int(os.popen2("sysctl -n hw.ncpu")[1].read())
  # Windows:
  if os.environ.has_key('NUMBER_OF_PROCESSORS'):
    return max(int(os.environ.get('NUMBER_OF_PROCESSORS', '1')), 1)
  return 1  # Default

# Support PROGRESS= to show progress in different ways.
p = ARGUMENTS.get('PROGRESS')
if p == 'spinner':
  Progress(['/\\r', '|\\r', '\\\\\\r', '-\\r'],
           interval=5,
           file=open('/dev/tty', 'w'))
elif p == 'name':
  Progress('$TARGET\\r', overwrite=True, file=open('/dev/tty', 'w'))

# Set the default -j value based on the number of processors.
SetOption('num_jobs', GetProcessorCount() + 1)

# Have SCons use its cached dependency information.
SetOption('implicit_cache', 1)

# Only re-calculate MD5 checksums if a timestamp has changed.
Decider('MD5-timestamp')

# Since we set the -j value by default, suppress SCons warnings about being
# unable to support parallel build on versions of Python with no threading.
default_warnings = ['no-no-parallel-support']
SetOption('warn', default_warnings + GetOption('warn'))

AddOption('--mode', nargs=1, dest='conf_list', default=[],
          action='append', help='Configuration to build.')

AddOption('--verbose', dest='verbose', default=False,
          action='store_true', help='Verbose command-line output.')


#
sconscript_file_map = %(sconscript_files)s

class LoadTarget:
  '''
  Class for deciding if a given target sconscript is to be included
  based on a list of included target names, optionally prefixed with '-'
  to exclude a target name.
  '''
  def __init__(self, load):
    '''
    Initialize a class with a list of names for possible loading.

    Arguments:
      load:  list of elements in the LOAD= specification
    '''
    self.included = set([c for c in load if not c.startswith('-')])
    self.excluded = set([c[1:] for c in load if c.startswith('-')])

    if not self.included:
      self.included = set(['all'])

  def __call__(self, target):
    '''
    Returns True if the specified target's sconscript file should be
    loaded, based on the initialized included and excluded lists.
    '''
    return (target in self.included or
            ('all' in self.included and not target in self.excluded))

if 'LOAD' in ARGUMENTS:
  load = ARGUMENTS['LOAD'].split(',')
else:
  load = []
load_target = LoadTarget(load)

sconscript_files = []
for target, sconscript in sconscript_file_map.iteritems():
  if load_target(target):
    sconscript_files.append(sconscript)


target_alias_list= []

conf_list = GetOption('conf_list')
if conf_list:
    # In case the same --mode= value was specified multiple times.
    conf_list = list(set(conf_list))
else:
    conf_list = [%(default_configuration)r]

sconsbuild_dir = Dir(%(sconsbuild_dir)s)


def FilterOut(self, **kw):
  kw = SCons.Environment.copy_non_reserved_keywords(kw)
  for key, val in kw.items():
    envval = self.get(key, None)
    if envval is None:
      # No existing variable in the environment, so nothing to delete.
      continue

    for vremove in val:
      # Use while not if, so we can handle duplicates.
      while vremove in envval:
        envval.remove(vremove)

    self[key] = envval

    # TODO(sgk): SCons.Environment.Append() has much more logic to deal
    # with various types of values.  We should handle all those cases in here
    # too.  (If variable is a dict, etc.)


non_compilable_suffixes = {
    'LINUX' : set([
        '.bdic',
        '.css',
        '.dat',
        '.fragment',
        '.gperf',
        '.h',
        '.hh',
        '.hpp',
        '.html',
        '.hxx',
        '.idl',
        '.in',
        '.in0',
        '.in1',
        '.js',
        '.mk',
        '.rc',
        '.sigs',
        '',
    ]),
    'WINDOWS' : set([
        '.h',
        '.hh',
        '.hpp',
        '.dat',
        '.idl',
        '.in',
        '.in0',
        '.in1',
    ]),
}

def compilable(env, file):
  base, ext = os.path.splitext(str(file))
  if ext in non_compilable_suffixes[env['TARGET_PLATFORM']]:
    return False
  return True

def compilable_files(env, sources):
  return [x for x in sources if compilable(env, x)]

def GypProgram(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  result = env.Program(target, source, *args, **kw)
  if env.get('INCREMENTAL'):
    env.Precious(result)
  return result

def GypTestProgram(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  result = env.Program(target, source, *args, **kw)
  if env.get('INCREMENTAL'):
    env.Precious(*result)
  return result

def GypLibrary(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  result = env.Library(target, source, *args, **kw)
  return result

def GypLoadableModule(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  result = env.LoadableModule(target, source, *args, **kw)
  return result

def GypStaticLibrary(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  result = env.StaticLibrary(target, source, *args, **kw)
  return result

def GypSharedLibrary(env, target, source, *args, **kw):
  source = compilable_files(env, source)
  result = env.SharedLibrary(target, source, *args, **kw)
  if env.get('INCREMENTAL'):
    env.Precious(result)
  return result

def add_gyp_methods(env):
  env.AddMethod(GypProgram)
  env.AddMethod(GypTestProgram)
  env.AddMethod(GypLibrary)
  env.AddMethod(GypLoadableModule)
  env.AddMethod(GypStaticLibrary)
  env.AddMethod(GypSharedLibrary)

  env.AddMethod(FilterOut)

  env.AddMethod(compilable)


base_env = Environment(
    tools = %(scons_tools)s,
    INTERMEDIATE_DIR='$OBJ_DIR/${COMPONENT_NAME}/_${TARGET_NAME}_intermediate',
    LIB_DIR='$TOP_BUILDDIR/lib',
    OBJ_DIR='$TOP_BUILDDIR/obj',
    SCONSBUILD_DIR=sconsbuild_dir.abspath,
    SHARED_INTERMEDIATE_DIR='$OBJ_DIR/_global_intermediate',
    SRC_DIR=Dir(%(src_dir)r),
    TARGET_PLATFORM='LINUX',
    TOP_BUILDDIR='$SCONSBUILD_DIR/$CONFIG_NAME',
    LIBPATH=['$LIB_DIR'],
)

if not GetOption('verbose'):
  base_env.SetDefault(
      ARCOMSTR='Creating library $TARGET',
      ASCOMSTR='Assembling $TARGET',
      CCCOMSTR='Compiling $TARGET',
      CONCATSOURCECOMSTR='ConcatSource $TARGET',
      CXXCOMSTR='Compiling $TARGET',
      LDMODULECOMSTR='Building loadable module $TARGET',
      LINKCOMSTR='Linking $TARGET',
      MANIFESTCOMSTR='Updating manifest for $TARGET',
      MIDLCOMSTR='Compiling IDL $TARGET',
      PCHCOMSTR='Precompiling $TARGET',
      RANLIBCOMSTR='Indexing $TARGET',
      RCCOMSTR='Compiling resource $TARGET',
      SHCCCOMSTR='Compiling $TARGET',
      SHCXXCOMSTR='Compiling $TARGET',
      SHLINKCOMSTR='Linking $TARGET',
      SHMANIFESTCOMSTR='Updating manifest for $TARGET',
  )

add_gyp_methods(base_env)

for conf in conf_list:
  env = base_env.Clone(CONFIG_NAME=conf)
  SConsignFile(env.File('$TOP_BUILDDIR/.sconsign').abspath)
  for sconscript in sconscript_files:
    target_alias = env.SConscript(sconscript, exports=['env'])
    if target_alias:
      target_alias_list.extend(target_alias)

Default(Alias('all', target_alias_list))

help_fmt = '''
Usage: hammer [SCONS_OPTIONS] [VARIABLES] [TARGET] ...

Local command-line build options:
  --mode=CONFIG             Configuration to build:
                              --mode=Debug [default]
                              --mode=Release
  --verbose                 Print actual executed command lines.

Supported command-line build variables:
  LOAD=[module,...]         Comma-separated list of components to load in the
                              dependency graph ('-' prefix excludes)
  PROGRESS=type             Display a progress indicator:
                              name:  print each evaluated target name
                              spinner:  print a spinner every 5 targets

The following TARGET names can also be used as LOAD= module names:

%%s
'''

if GetOption('help'):
  def columnar_text(items, width=78, indent=2, sep=2):
    result = []
    colwidth = max(map(len, items)) + sep
    cols = (width - indent) / colwidth
    if cols < 1:
      cols = 1
    rows = (len(items) + cols - 1) / cols
    indent = '%%*s' %% (indent, '')
    sep = indent
    for row in xrange(0, rows):
      result.append(sep)
      for i in xrange(row, len(items), rows):
        result.append('%%-*s' %% (colwidth, items[i]))
      sep = '\\n' + indent
    result.append('\\n')
    return ''.join(result)

  load_list = set(sconscript_file_map.keys())
  target_aliases = set(map(str, target_alias_list))

  common = load_list and target_aliases
  load_only = load_list - common
  target_only = target_aliases - common
  help_text = [help_fmt %% columnar_text(sorted(list(common)))]
  if target_only:
    fmt = "The following are additional TARGET names:\\n\\n%%s\\n"
    help_text.append(fmt %% columnar_text(sorted(list(target_only))))
  if load_only:
    fmt = "The following are additional LOAD= module names:\\n\\n%%s\\n"
    help_text.append(fmt %% columnar_text(sorted(list(load_only))))
  Help(''.join(help_text))
"""

# TEMPLATE END
#############################################################################


def GenerateSConscriptWrapper(build_file, build_file_data, name,
                              output_filename, sconscript_files,
                              default_configuration):
  """
  Generates the "wrapper" SConscript file (analogous to the Visual Studio
  solution) that calls all the individual target SConscript files.
  """
  output_dir = os.path.dirname(output_filename)
  src_dir = build_file_data['_DEPTH']
  src_dir_rel = gyp.common.RelativePath(src_dir, output_dir)
  if not src_dir_rel:
    src_dir_rel = '.'
  scons_settings = build_file_data.get('scons_settings', {})
  sconsbuild_dir = scons_settings.get('sconsbuild_dir', '#')
  scons_tools = scons_settings.get('tools', ['default'])

  sconscript_file_lines = ['dict(']
  for target in sorted(sconscript_files.keys()):
    sconscript = sconscript_files[target]
    sconscript_file_lines.append('    %s = %r,' % (target, sconscript))
  sconscript_file_lines.append(')')

  fp = open(output_filename, 'w')
  fp.write(header)
  fp.write(_wrapper_template % {
               'default_configuration' : default_configuration,
               'name' : name,
               'scons_tools' : repr(scons_tools),
               'sconsbuild_dir' : repr(sconsbuild_dir),
               'sconscript_files' : '\n'.join(sconscript_file_lines),
               'src_dir' : src_dir_rel,
           })
  fp.close()

  # Generate the SConstruct file that invokes the wrapper SConscript.
  dir, fname = os.path.split(output_filename)
  SConstruct = os.path.join(dir, 'SConstruct')
  fp = open(SConstruct, 'w')
  fp.write(header)
  fp.write('SConscript(%s)\n' % repr(fname))
  fp.close()


def TargetFilename(target, build_file=None, output_suffix=''):
  """Returns the .scons file name for the specified target.
  """
  if build_file is None:
    build_file, target = gyp.common.ParseQualifiedTarget(target)[:2]
  output_file = os.path.join(os.path.dirname(build_file),
                             target + output_suffix + '.scons')
  return output_file


def GenerateOutput(target_list, target_dicts, data, params):
  """
  Generates all the output files for the specified targets.
  """
  options = params['options']

  if options.generator_output:
    def output_path(filename):
      return filename.replace(params['cwd'], options.generator_output)
  else:
    def output_path(filename):
      return filename

  default_configuration = None

  for qualified_target in target_list:
    spec = target_dicts[qualified_target]
    if spec['toolset'] != 'target':
      raise Exception(
          'Multiple toolsets not supported in scons build (target %s)' %
          qualified_target)
    scons_target = SCons.Target(spec)
    if scons_target.is_ignored:
      continue

    # TODO:  assumes the default_configuration of the first target
    # non-Default target is the correct default for all targets.
    # Need a better model for handle variation between targets.
    if (not default_configuration and
        spec['default_configuration'] != 'Default'):
      default_configuration = spec['default_configuration']

    build_file, target = gyp.common.ParseQualifiedTarget(qualified_target)[:2]
    output_file = TargetFilename(target, build_file, options.suffix)
    if options.generator_output:
      output_file = output_path(output_file)

    if not spec.has_key('libraries'):
      spec['libraries'] = []

    # Add dependent static library targets to the 'libraries' value.
    deps = spec.get('dependencies', [])
    spec['scons_dependencies'] = []
    for d in deps:
      td = target_dicts[d]
      target_name = td['target_name']
      spec['scons_dependencies'].append("Alias('%s')" % target_name)
      if td['type'] in ('static_library', 'shared_library'):
        libname = td.get('product_name', target_name)
        spec['libraries'].append('lib' + libname)
      if td['type'] == 'loadable_module':
        prereqs = spec.get('scons_prerequisites', [])
        # TODO:  parameterize with <(SHARED_LIBRARY_*) variables?
        td_target = SCons.Target(td)
        td_target.target_prefix = '${SHLIBPREFIX}'
        td_target.target_suffix = '${SHLIBSUFFIX}'

    GenerateSConscript(output_file, spec, build_file, data[build_file])

  if not default_configuration:
    default_configuration = 'Default'

  for build_file in sorted(data.keys()):
    path, ext = os.path.splitext(build_file)
    if ext != '.gyp':
      continue
    output_dir, basename = os.path.split(path)
    output_filename  = path + '_main' + options.suffix + '.scons'

    all_targets = gyp.common.AllTargets(target_list, target_dicts, build_file)
    sconscript_files = {}
    for t in all_targets:
      scons_target = SCons.Target(target_dicts[t])
      if scons_target.is_ignored:
        continue
      bf, target = gyp.common.ParseQualifiedTarget(t)[:2]
      target_filename = TargetFilename(target, bf, options.suffix)
      tpath = gyp.common.RelativePath(target_filename, output_dir)
      sconscript_files[target] = tpath

    output_filename = output_path(output_filename)
    if sconscript_files:
      GenerateSConscriptWrapper(build_file, data[build_file], basename,
                                output_filename, sconscript_files,
                                default_configuration)
