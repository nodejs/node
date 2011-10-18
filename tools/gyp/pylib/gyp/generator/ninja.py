#!/usr/bin/python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gyp
import gyp.common
import gyp.system_test
import os.path
import pprint
import subprocess
import sys

import gyp.ninja_syntax as ninja_syntax

generator_default_variables = {
  'OS': 'linux',

  'EXECUTABLE_PREFIX': '',
  'EXECUTABLE_SUFFIX': '',
  'STATIC_LIB_PREFIX': '',
  'STATIC_LIB_SUFFIX': '.a',
  'SHARED_LIB_PREFIX': 'lib',
  'SHARED_LIB_SUFFIX': '.so',

  # Gyp expects the following variables to be expandable by the build
  # system to the appropriate locations.  Ninja prefers paths to be
  # known at compile time.  To resolve this, introduce special
  # variables starting with $! (which begin with a $ so gyp knows it
  # should be treated as a path, but is otherwise an invalid
  # ninja/shell variable) that are passed to gyp here but expanded
  # before writing out into the target .ninja files; see
  # ExpandSpecial.
  'INTERMEDIATE_DIR': '$!INTERMEDIATE_DIR',
  'SHARED_INTERMEDIATE_DIR': '$!PRODUCT_DIR/gen',
  'PRODUCT_DIR': '$!PRODUCT_DIR',
  'SHARED_LIB_DIR': '$!PRODUCT_DIR/lib',
  'LIB_DIR': '',

  # Special variables that may be used by gyp 'rule' targets.
  # We generate definitions for these variables on the fly when processing a
  # rule.
  'RULE_INPUT_ROOT': '${root}',
  'RULE_INPUT_DIRNAME': '${dirname}',
  'RULE_INPUT_PATH': '${source}',
  'RULE_INPUT_EXT': '${ext}',
  'RULE_INPUT_NAME': '${name}',
}

# TODO: enable cross compiling once we figure out:
# - how to not build extra host objects in the non-cross-compile case.
# - how to decide what the host compiler is (should not just be $cc).
# - need ld_host as well.
generator_supports_multiple_toolsets = False


def StripPrefix(arg, prefix):
  if arg.startswith(prefix):
    return arg[len(prefix):]
  return arg


def QuoteShellArgument(arg):
  return "'" + arg.replace("'", "'" + '"\'"' + "'")  + "'"


def MaybeQuoteShellArgument(arg):
  if '"' in arg or ' ' in arg:
    return QuoteShellArgument(arg)
  return arg


def InvertRelativePath(path):
  """Given a relative path like foo/bar, return the inverse relative path:
  the path from the relative path back to the origin dir.

  E.g. os.path.normpath(os.path.join(path, InvertRelativePath(path)))
  should always produce the empty string."""

  if not path:
    return path
  # Only need to handle relative paths into subdirectories for now.
  assert '..' not in path, path
  depth = len(path.split('/'))
  return '/'.join(['..'] * depth)


# A small discourse on paths as used within the Ninja build:
# All files we produce (both at gyp and at build time) appear in the
# build directory (e.g. out/Debug).
#
# Paths within a given .gyp file are always relative to the directory
# containing the .gyp file.  Call these "gyp paths".  This includes
# sources as well as the starting directory a given gyp rule/action
# expects to be run from.  We call the path from the source root to
# the gyp file the "base directory" within the per-.gyp-file
# NinjaWriter code.
#
# All paths as written into the .ninja files are relative to the build
# directory.  Call these paths "ninja paths".
#
# We translate between these two notions of paths with two helper
# functions:
#
# - GypPathToNinja translates a gyp path (i.e. relative to the .gyp file)
#   into the equivalent ninja path.
#
# - GypPathToUniqueOutput translates a gyp path into a ninja path to write
#   an output file; the result can be namespaced such that is unique
#   to the input file name as well as the output target name.

class NinjaWriter:
  def __init__(self, target_outputs, base_dir, build_dir, output_file):
    """
    base_dir: path from source root to directory containing this gyp file,
              by gyp semantics, all input paths are relative to this
    build_dir: path from source root to build output
    """

    self.target_outputs = target_outputs
    self.base_dir = base_dir
    self.build_dir = build_dir
    self.ninja = ninja_syntax.Writer(output_file)

    # Relative path from build output dir to base dir.
    self.build_to_base = os.path.join(InvertRelativePath(build_dir), base_dir)
    # Relative path from base dir to build dir.
    self.base_to_build = os.path.join(InvertRelativePath(base_dir), build_dir)

  def ExpandSpecial(self, path, product_dir=None):
    """Expand specials like $!PRODUCT_DIR in |path|.

    If |product_dir| is None, assumes the cwd is already the product
    dir.  Otherwise, |product_dir| is the relative path to the product
    dir.
    """

    PRODUCT_DIR = '$!PRODUCT_DIR'
    if PRODUCT_DIR in path:
      if product_dir:
        path = path.replace(PRODUCT_DIR, product_dir)
      else:
        path = path.replace(PRODUCT_DIR + '/', '')
        path = path.replace(PRODUCT_DIR, '.')

    INTERMEDIATE_DIR = '$!INTERMEDIATE_DIR'
    if INTERMEDIATE_DIR in path:
      int_dir = self.GypPathToUniqueOutput('gen')
      # GypPathToUniqueOutput generates a path relative to the product dir,
      # so insert product_dir in front if it is provided.
      path = path.replace(INTERMEDIATE_DIR,
                          os.path.join(product_dir or '', int_dir))

    return path

  def GypPathToNinja(self, path):
    """Translate a gyp path to a ninja path.

    See the above discourse on path conversions."""
    if path.startswith('$!'):
      return self.ExpandSpecial(path)
    assert '$' not in path, path
    return os.path.normpath(os.path.join(self.build_to_base, path))

  def GypPathToUniqueOutput(self, path, qualified=True):
    """Translate a gyp path to a ninja path for writing output.

    If qualified is True, qualify the resulting filename with the name
    of the target.  This is necessary when e.g. compiling the same
    path twice for two separate output targets.

    See the above discourse on path conversions."""

    path = self.ExpandSpecial(path)
    assert not path.startswith('$'), path

    # Translate the path following this scheme:
    #   Input: foo/bar.gyp, target targ, references baz/out.o
    #   Output: obj/foo/baz/targ.out.o (if qualified)
    #           obj/foo/baz/out.o (otherwise)
    #     (and obj.host instead of obj for cross-compiles)
    #
    # Why this scheme and not some other one?
    # 1) for a given input, you can compute all derived outputs by matching
    #    its path, even if the input is brought via a gyp file with '..'.
    # 2) simple files like libraries and stamps have a simple filename.

    obj = 'obj'
    if self.toolset != 'target':
      obj += '.' + self.toolset

    path_dir, path_basename = os.path.split(path)
    if qualified:
      path_basename = self.name + '.' + path_basename
    return os.path.normpath(os.path.join(obj, self.base_dir, path_dir,
                                         path_basename))

  def WriteCollapsedDependencies(self, name, targets):
    """Given a list of targets, return a dependency list for a single
    file representing the result of building all the targets.

    Uses a stamp file if necessary."""

    if len(targets) > 1:
      stamp = self.GypPathToUniqueOutput(name + '.stamp')
      targets = self.ninja.build(stamp, 'stamp', targets)
      self.ninja.newline()
    return targets

  def WriteSpec(self, spec, config):
    """The main entry point for NinjaWriter: write the build rules for a spec.

    Returns the path to the build output, or None."""

    self.name = spec['target_name']
    self.toolset = spec['toolset']

    if spec['type'] == 'settings':
      # TODO: 'settings' is not actually part of gyp; it was
      # accidentally introduced somehow into just the Linux build files.
      # Remove this (or make it an error) once all the users are fixed.
      print ("WARNING: %s uses invalid type 'settings'.  " % self.name +
             "Please fix the source gyp file to use type 'none'.")
      print "See http://code.google.com/p/chromium/issues/detail?id=96629 ."
      spec['type'] = 'none'

    # Compute predepends for all rules.
    # prebuild is the dependencies this target depends on before
    # running any of its internal steps.
    prebuild = []
    if 'dependencies' in spec:
      for dep in spec['dependencies']:
        if dep in self.target_outputs:
          prebuild.append(self.target_outputs[dep][0])
      prebuild = self.WriteCollapsedDependencies('predepends', prebuild)

    # Write out actions, rules, and copies.  These must happen before we
    # compile any sources, so compute a list of predependencies for sources
    # while we do it.
    extra_sources = []
    sources_predepends = self.WriteActionsRulesCopies(spec, extra_sources,
                                                      prebuild)

    # Write out the compilation steps, if any.
    link_deps = []
    sources = spec.get('sources', []) + extra_sources
    if sources:
      link_deps = self.WriteSources(config, sources,
                                    sources_predepends or prebuild)
      # Some actions/rules output 'sources' that are already object files.
      link_deps += [self.GypPathToNinja(f) for f in sources if f.endswith('.o')]

    # The final output of our target depends on the last output of the
    # above steps.
    output = None
    final_deps = link_deps or sources_predepends or prebuild
    if final_deps:
      output = self.WriteTarget(spec, config, final_deps)
      if self.name != output and self.toolset == 'target':
        # Write a short name to build this target.  This benefits both the
        # "build chrome" case as well as the gyp tests, which expect to be
        # able to run actions and build libraries by their short name.
        self.ninja.build(self.name, 'phony', output)
    return output

  def WriteActionsRulesCopies(self, spec, extra_sources, prebuild):
    """Write out the Actions, Rules, and Copies steps.  Return any outputs
    of these steps (or a stamp file if there are lots of outputs)."""
    outputs = []

    if 'actions' in spec:
      outputs += self.WriteActions(spec['actions'], extra_sources, prebuild)
    if 'rules' in spec:
      outputs += self.WriteRules(spec['rules'], extra_sources, prebuild)
    if 'copies' in spec:
      outputs += self.WriteCopies(spec['copies'], prebuild)

    outputs = self.WriteCollapsedDependencies('actions_rules_copies', outputs)

    return outputs

  def GenerateDescription(self, verb, message, fallback):
    """Generate and return a description of a build step.

    |verb| is the short summary, e.g. ACTION or RULE.
    |message| is a hand-written description, or None if not available.
    |fallback| is the gyp-level name of the step, usable as a fallback.
    """
    if self.toolset != 'target':
      verb += '(%s)' % self.toolset
    if message:
      return '%s %s' % (verb, self.ExpandSpecial(message))
    else:
      return '%s %s: %s' % (verb, self.name, fallback)

  def WriteActions(self, actions, extra_sources, prebuild):
    all_outputs = []
    for action in actions:
      # First write out a rule for the action.
      name = action['action_name']
      description = self.GenerateDescription('ACTION',
                                             action.get('message', None),
                                             name)
      rule_name = self.WriteNewNinjaRule(name, action['action'], description)

      inputs = [self.GypPathToNinja(i) for i in action['inputs']]
      if int(action.get('process_outputs_as_sources', False)):
        extra_sources += action['outputs']
      outputs = [self.GypPathToNinja(o) for o in action['outputs']]

      # Then write out an edge using the rule.
      self.ninja.build(outputs, rule_name, inputs,
                       order_only=prebuild)
      all_outputs += outputs

      self.ninja.newline()

    return all_outputs

  def WriteRules(self, rules, extra_sources, prebuild):
    all_outputs = []
    for rule in rules:
      # First write out a rule for the rule action.
      name = rule['rule_name']
      args = rule['action']
      description = self.GenerateDescription(
          'RULE',
          rule.get('message', None),
          ('%s ' + generator_default_variables['RULE_INPUT_PATH']) % name)
      rule_name = self.WriteNewNinjaRule(name, args, description)

      # TODO: if the command references the outputs directly, we should
      # simplify it to just use $out.

      # Rules can potentially make use of some special variables which
      # must vary per source file.
      # Compute the list of variables we'll need to provide.
      special_locals = ('source', 'root', 'dirname', 'ext', 'name')
      needed_variables = set(['source'])
      for argument in args:
        for var in special_locals:
          if ('${%s}' % var) in argument:
            needed_variables.add(var)

      # For each source file, write an edge that generates all the outputs.
      for source in rule.get('rule_sources', []):
        dirname, basename = os.path.split(source)
        root, ext = os.path.splitext(basename)

        # Gather the list of outputs, expanding $vars if possible.
        outputs = []
        for output in rule['outputs']:
          outputs.append(output.replace(
              generator_default_variables['RULE_INPUT_ROOT'], root).replace(
                  generator_default_variables['RULE_INPUT_DIRNAME'], dirname))

        if int(rule.get('process_outputs_as_sources', False)):
          extra_sources += outputs

        extra_bindings = []
        for var in needed_variables:
          if var == 'root':
            extra_bindings.append(('root', root))
          elif var == 'dirname':
            extra_bindings.append(('dirname', dirname))
          elif var == 'source':
            # '$source' is a parameter to the rule action, which means
            # it shouldn't be converted to a Ninja path.  But we don't
            # want $!PRODUCT_DIR in there either.
            source_expanded = self.ExpandSpecial(source, self.base_to_build)
            extra_bindings.append(('source', source_expanded))
          elif var == 'ext':
            extra_bindings.append(('ext', ext))
          elif var == 'name':
            extra_bindings.append(('name', basename))
          else:
            assert var == None, repr(var)

        inputs = map(self.GypPathToNinja, rule.get('inputs', []))
        outputs = map(self.GypPathToNinja, outputs)
        self.ninja.build(outputs, rule_name, self.GypPathToNinja(source),
                         implicit=inputs,
                         order_only=prebuild,
                         variables=extra_bindings)

        all_outputs.extend(outputs)

    return all_outputs

  def WriteCopies(self, copies, prebuild):
    outputs = []
    for copy in copies:
      for path in copy['files']:
        # Normalize the path so trailing slashes don't confuse us.
        path = os.path.normpath(path)
        basename = os.path.split(path)[1]
        src = self.GypPathToNinja(path)
        dst = self.GypPathToNinja(os.path.join(copy['destination'], basename))
        outputs += self.ninja.build(dst, 'copy', src,
                                    order_only=prebuild)

    return outputs

  def WriteSources(self, config, sources, predepends):
    """Write build rules to compile all of |sources|."""
    if self.toolset == 'host':
      self.ninja.variable('cc', '$cc_host')
      self.ninja.variable('cxx', '$cxx_host')

    self.WriteVariableList('defines',
        ['-D' + MaybeQuoteShellArgument(ninja_syntax.escape(d))
         for d in config.get('defines', [])])
    self.WriteVariableList('includes',
                           ['-I' + self.GypPathToNinja(i)
                            for i in config.get('include_dirs', [])])
    self.WriteVariableList('cflags', config.get('cflags'))
    self.WriteVariableList('cflags_c', config.get('cflags_c'))
    self.WriteVariableList('cflags_cc', config.get('cflags_cc'))
    self.ninja.newline()
    outputs = []
    for source in sources:
      filename, ext = os.path.splitext(source)
      ext = ext[1:]
      if ext in ('cc', 'cpp', 'cxx'):
        command = 'cxx'
      elif ext in ('c', 's', 'S'):
        command = 'cc'
      else:
        # TODO: should we assert here on unexpected extensions?
        continue
      input = self.GypPathToNinja(source)
      output = self.GypPathToUniqueOutput(filename + '.o')
      self.ninja.build(output, command, input,
                       order_only=predepends)
      outputs.append(output)
    self.ninja.newline()
    return outputs

  def WriteTarget(self, spec, config, final_deps):
    if spec['type'] == 'none':
      # This target doesn't have any explicit final output, but is instead
      # used for its effects before the final output (e.g. copies steps).
      # Reuse the existing output if it's easy.
      if len(final_deps) == 1:
        return final_deps[0]
      # Otherwise, fall through to writing out a stamp file.

    output = self.ComputeOutput(spec)

    output_uses_linker = spec['type'] in ('executable', 'loadable_module',
                                          'shared_library')

    implicit_deps = set()
    if 'dependencies' in spec:
      # Two kinds of dependencies:
      # - Linkable dependencies (like a .a or a .so): add them to the link line.
      # - Non-linkable dependencies (like a rule that generates a file
      #   and writes a stamp file): add them to implicit_deps
      if output_uses_linker:
        extra_deps = set()
        for dep in spec['dependencies']:
          input, linkable = self.target_outputs.get(dep, (None, False))
          if not input:
            continue
          if linkable:
            extra_deps.add(input)
          else:
            # TODO: Chrome-specific HACK.  Chrome runs this lastchange rule on
            # every build, but we don't want to rebuild when it runs.
            if 'lastchange' not in input:
              implicit_deps.add(input)
        final_deps.extend(list(extra_deps))
    command_map = {
      'executable':      'link',
      'static_library':  'alink',
      'loadable_module': 'solink_module',
      'shared_library':  'solink',
      'none':            'stamp',
    }
    command = command_map[spec['type']]

    if output_uses_linker:
      self.WriteVariableList('ldflags',
                             gyp.common.uniquer(map(self.ExpandSpecial,
                                                    config.get('ldflags', []))))
      self.WriteVariableList('libs',
                             gyp.common.uniquer(map(self.ExpandSpecial,
                                                    spec.get('libraries', []))))

    extra_bindings = []
    if command in ('solink', 'solink_module'):
      extra_bindings.append(('soname', os.path.split(output)[1]))

    self.ninja.build(output, command, final_deps,
                     implicit=list(implicit_deps),
                     variables=extra_bindings)

    return output

  def ComputeOutputFileName(self, spec):
    """Compute the filename of the final output for the current target."""

    # Compute filename prefix: the product prefix, or a default for
    # the product type.
    DEFAULT_PREFIX = {
      'loadable_module': 'lib',
      'shared_library': 'lib',
      }
    prefix = spec.get('product_prefix', DEFAULT_PREFIX.get(spec['type'], ''))

    # Compute filename extension: the product extension, or a default
    # for the product type.
    DEFAULT_EXTENSION = {
      'static_library': 'a',
      'loadable_module': 'so',
      'shared_library': 'so',
      }
    extension = spec.get('product_extension',
                         DEFAULT_EXTENSION.get(spec['type'], ''))
    if extension:
      extension = '.' + extension

    if 'product_name' in spec:
      # If we were given an explicit name, use that.
      target = spec['product_name']
    else:
      # Otherwise, derive a name from the target name.
      target = spec['target_name']
      if prefix == 'lib':
        # Snip out an extra 'lib' from libs if appropriate.
        target = StripPrefix(target, 'lib')

    if spec['type'] in ('static_library', 'loadable_module', 'shared_library',
                        'executable'):
      return '%s%s%s' % (prefix, target, extension)
    elif spec['type'] == 'none':
      return '%s.stamp' % target
    else:
      raise 'Unhandled output type', spec['type']

  def ComputeOutput(self, spec):
    """Compute the path for the final output of the spec."""

    filename = self.ComputeOutputFileName(spec)

    if 'product_dir' in spec:
      path = os.path.join(spec['product_dir'], filename)
      return self.ExpandSpecial(path)

    # Executables and loadable modules go into the output root,
    # libraries go into shared library dir, and everything else
    # goes into the normal place.
    if spec['type'] in ('executable', 'loadable_module'):
      return filename
    elif spec['type'] == 'shared_library':
      libdir = 'lib'
      if self.toolset != 'target':
        libdir = 'lib/%s' % self.toolset
      return os.path.join(libdir, filename)
    else:
      return self.GypPathToUniqueOutput(filename, qualified=False)

  def WriteVariableList(self, var, values):
    if values is None:
      values = []
    self.ninja.variable(var, ' '.join(values))

  def WriteNewNinjaRule(self, name, args, description):
    """Write out a new ninja "rule" statement for a given command.

    Returns the name of the new rule."""

    # TODO: we shouldn't need to qualify names; we do it because
    # currently the ninja rule namespace is global, but it really
    # should be scoped to the subninja.
    rule_name = self.name
    if self.toolset == 'target':
      rule_name += '.' + self.toolset
    rule_name += '.' + name
    rule_name = rule_name.replace(' ', '_')

    args = args[:]

    # gyp dictates that commands are run from the base directory.
    # cd into the directory before running, and adjust paths in
    # the arguments to point to the proper locations.
    cd = 'cd %s; ' % self.build_to_base
    args = [self.ExpandSpecial(arg, self.base_to_build) for arg in args]

    command = cd + gyp.common.EncodePOSIXShellList(args)
    self.ninja.rule(rule_name, command, description)
    self.ninja.newline()

    return rule_name


def CalculateVariables(default_variables, params):
  """Calculate additional variables for use in the build (called by gyp)."""
  cc_target = os.environ.get('CC.target', os.environ.get('CC', 'cc'))
  default_variables['LINKER_SUPPORTS_ICF'] = \
      gyp.system_test.TestLinkerSupportsICF(cc_command=cc_target)


def OpenOutput(path):
  """Open |path| for writing, creating directories if necessary."""
  try:
    os.makedirs(os.path.dirname(path))
  except OSError:
    pass
  return open(path, 'w')


def GenerateOutput(target_list, target_dicts, data, params):
  options = params['options']
  generator_flags = params.get('generator_flags', {})

  if options.generator_output:
    raise NotImplementedError, "--generator_output not implemented for ninja"

  config_name = generator_flags.get('config', None)
  if config_name is None:
    # Guess which config we want to use: pick the first one from the
    # first target.
    config_name = target_dicts[target_list[0]]['default_configuration']

  # builddir: relative path from source root to our output files.
  # e.g. "out/Debug"
  builddir = os.path.join(generator_flags.get('output_dir', 'out'), config_name)

  master_ninja = ninja_syntax.Writer(
      OpenOutput(os.path.join(options.toplevel_dir, builddir, 'build.ninja')),
      width=120)

  # TODO: compute cc/cxx/ld/etc. by command-line arguments and system tests.
  master_ninja.variable('cc', os.environ.get('CC', 'gcc'))
  master_ninja.variable('cxx', os.environ.get('CXX', 'g++'))
  master_ninja.variable('ld', '$cxx -Wl,--threads -Wl,--thread-count=4')
  master_ninja.variable('cc_host', '$cc')
  master_ninja.variable('cxx_host', '$cxx')
  master_ninja.newline()

  master_ninja.rule(
    'cc',
    description='CC $out',
    command=('$cc -MMD -MF $out.d $defines $includes $cflags $cflags_c '
             '-c $in -o $out'),
    depfile='$out.d')
  master_ninja.rule(
    'cxx',
    description='CXX $out',
    command=('$cxx -MMD -MF $out.d $defines $includes $cflags $cflags_cc '
             '-c $in -o $out'),
    depfile='$out.d')
  master_ninja.rule(
    'alink',
    description='AR $out',
    command='rm -f $out && ar rcsT $out $in')
  master_ninja.rule(
    'solink',
    description='SOLINK $out',
    command=('$ld -shared $ldflags -o $out -Wl,-soname=$soname '
             '-Wl,--whole-archive $in -Wl,--no-whole-archive $libs'))
  master_ninja.rule(
    'solink_module',
    description='SOLINK(module) $out',
    command=('$ld -shared $ldflags -o $out -Wl,-soname=$soname '
             '-Wl,--start-group $in -Wl,--end-group $libs'))
  master_ninja.rule(
    'link',
    description='LINK $out',
    command=('$ld $ldflags -o $out -Wl,-rpath=\$$ORIGIN/lib '
             '-Wl,--start-group $in -Wl,--end-group $libs'))
  master_ninja.rule(
    'stamp',
    description='STAMP $out',
    command='touch $out')
  master_ninja.rule(
    'copy',
    description='COPY $in $out',
    command='ln -f $in $out 2>/dev/null || cp -af $in $out')
  master_ninja.newline()

  all_targets = set()
  for build_file in params['build_files']:
    for target in gyp.common.AllTargets(target_list, target_dicts, build_file):
      all_targets.add(target)
  all_outputs = set()

  target_outputs = {}
  for qualified_target in target_list:
    # qualified_target is like: third_party/icu/icu.gyp:icui18n#target
    build_file, name, toolset = \
        gyp.common.ParseQualifiedTarget(qualified_target)

    # TODO: what is options.depth and how is it different than
    # options.toplevel_dir?
    build_file = gyp.common.RelativePath(build_file, options.depth)

    base_path = os.path.dirname(build_file)
    obj = 'obj'
    if toolset != 'target':
      obj += '.' + toolset
    output_file = os.path.join(obj, base_path, name + '.ninja')
    spec = target_dicts[qualified_target]
    config = spec['configurations'][config_name]

    writer = NinjaWriter(target_outputs, base_path, builddir,
                         OpenOutput(os.path.join(options.toplevel_dir,
                                                 builddir,
                                                 output_file)))
    master_ninja.subninja(output_file)

    output = writer.WriteSpec(spec, config)
    if output:
      linkable = spec['type'] in ('static_library', 'shared_library')
      target_outputs[qualified_target] = (output, linkable)

      if qualified_target in all_targets:
        all_outputs.add(output)

  if all_outputs:
    master_ninja.build('all', 'phony', list(all_outputs))
