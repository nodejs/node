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
  # TODO: intermediate dir should *not* be shared between different targets.
  # Unfortunately, whatever we provide here gets written into many different
  # places within the gyp spec so it's difficult to make it target-specific.
  # Apparently we've made it this far with one global path for the make build
  # we're safe for now.
  'INTERMEDIATE_DIR': '$b/geni',
  'SHARED_INTERMEDIATE_DIR': '$b/gen',
  'PRODUCT_DIR': '$b',
  'SHARED_LIB_DIR': '$b/lib',
  'LIB_DIR': '$b',

  # Special variables that may be used by gyp 'rule' targets.
  # We generate definitions for these variables on the fly when processing a
  # rule.
  'RULE_INPUT_ROOT': '$root',
  'RULE_INPUT_PATH': '$source',
  'RULE_INPUT_EXT': '$ext',
  'RULE_INPUT_NAME': '$name',
}

NINJA_BASE = """\
builddir = %(builddir)s
# Short alias for builddir.
b = %(builddir)s

cc = %(cc)s
cxx = %(cxx)s

rule cc
  depfile = $out.d
  description = CC $out
  command = $cc -MMD -MF $out.d $defines $includes $cflags $cflags_c $
    -c $in -o $out

rule cxx
  depfile = $out.d
  description = CXX $out
  command = $cxx -MMD -MF $out.d $defines $includes $cflags $cflags_cc $
    -c $in -o $out

rule alink
  description = AR $out
  command = rm -f $out && ar rcsT $out $in

rule solink
  description = SOLINK $out
  command = g++ -Wl,--threads -Wl,--thread-count=4 $
    -shared $ldflags -o $out -Wl,-soname=$soname $
    -Wl,--whole-archive $in -Wl,--no-whole-archive $libs

rule link
  description = LINK $out
  command = g++ -Wl,--threads -Wl,--thread-count=4 $
    $ldflags -o $out -Wl,-rpath=\$$ORIGIN/lib $
    -Wl,--start-group $in -Wl,--end-group $libs

rule stamp
  description = STAMP $out
  command = touch $out

rule copy
  description = COPY $in $out
  command = ln -f $in $out 2>/dev/null || cp -af $in $out

"""


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


# A small discourse on paths as used within the Ninja build:
#
# Paths within a given .gyp file are always relative to the directory
# containing the .gyp file.  Call these "gyp paths".  This includes
# sources as well as the starting directory a given gyp rule/action
# expects to be run from.  We call this directory "base_dir" within
# the per-.gyp-file NinjaWriter code.
#
# All paths as written into the .ninja files are relative to the root
# of the tree.  Call these paths "ninja paths".  We set up the ninja
# variable "$b" to be the path to the root of the build output,
# e.g. out/Debug/.  All files we produce (both at gyp and at build
# time) appear in that output directory.
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
  def __init__(self, target_outputs, base_dir, output_file):
    self.target_outputs = target_outputs
    # The root-relative path to the source .gyp file; by gyp
    # semantics, all input paths are relative to this.
    self.base_dir = base_dir
    self.ninja = ninja_syntax.Writer(output_file)

  def GypPathToNinja(self, path):
    """Translate a gyp path to a ninja path.

    See the above discourse on path conversions."""
    if path.startswith('$'):
      # If the path contains a reference to a ninja variable, we know
      # it's already relative to the source root.
      return path
    return os.path.normpath(os.path.join(self.base_dir, path))

  def GypPathToUniqueOutput(self, path, qualified=False):
    """Translate a gyp path to a ninja path for writing output.

    If qualified is True, qualify the resulting filename with the name
    of the target.  This is necessary when e.g. compiling the same
    path twice for two separate output targets.

    See the above discourse on path conversions."""

    # It may seem strange to discard components of the path, but we are just
    # attempting to produce a known-unique output filename; we don't want to
    # reuse any global directory.
    genvars = generator_default_variables
    assert not genvars['SHARED_INTERMEDIATE_DIR'].startswith(
      genvars['INTERMEDIATE_DIR'])
    path = StripPrefix(path, genvars['INTERMEDIATE_DIR'])
    path = StripPrefix(path, genvars['SHARED_INTERMEDIATE_DIR'])
    path = StripPrefix(path, '/')
    assert not path.startswith('$')

    # Translate the path following this scheme:
    #   Input: foo/bar.gyp, target targ, references baz/out.o
    #   Output: $b/obj/foo/baz/targ.out.o (if qualified)
    #           $b/obj/foo/baz/out.o (otherwise)
    #
    # Why this scheme and not some other one?
    # 1) for a given input, you can compute all derived outputs by matching
    #    its path, even if the input is brought via a gyp file with '..'.
    # 2) simple files like libraries and stamps have a simple filename.
    path_dir, path_basename = os.path.split(path)
    if qualified:
      path_basename = self.name + '.' + path_basename
    return os.path.normpath(os.path.join('$b/obj', self.base_dir, path_dir,
                                         path_basename))

  def StampPath(self, name):
    """Return a path for a stamp file with a particular name.

    Stamp files are used to collapse a dependency on a bunch of files
    into a single file."""
    return self.GypPathToUniqueOutput(name + '.stamp', qualified=True)

  def WriteSpec(self, spec, config):
    """The main entry point for NinjaWriter: write the build rules for a spec.

    Returns the path to the build output, or None."""

    if spec['type'] == 'settings':
      # TODO: 'settings' is not actually part of gyp; it was
      # accidentally introduced somehow into just the Linux build files.
      return None

    self.name = spec['target_name']

    # Compute predepends for all rules.
    # prebuild is the dependencies this target depends on before
    # running any of its internal steps.
    prebuild = []
    if 'dependencies' in spec:
      prebuild_deps = []
      for dep in spec['dependencies']:
        if dep in self.target_outputs:
          prebuild_deps.append(self.target_outputs[dep][0])
      if prebuild_deps:
        stamp = self.StampPath('predepends')
        prebuild = self.ninja.build(stamp, 'stamp', prebuild_deps)
        self.ninja.newline()

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
      link_deps += [f for f in sources if f.endswith('.o')]

    # The final output of our target depends on the last output of the
    # above steps.
    final_deps = link_deps or sources_predepends or prebuild
    if final_deps:
      return self.WriteTarget(spec, config, final_deps)

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

    # To simplify downstream build edges, ensure we generate a single
    # stamp file that represents the results of all of the above.
    if len(outputs) > 1:
      stamp = self.StampPath('actions_rules_copies')
      outputs = self.ninja.build(stamp, 'stamp', outputs)

    return outputs

  def WriteActions(self, actions, extra_sources, prebuild):
    all_outputs = []
    for action in actions:
      # First write out a rule for the action.
      name = action['action_name']
      if 'message' in action:
        description = 'ACTION ' + action['message']
      else:
        description = 'ACTION %s: %s' % (self.name, action['action_name'])
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
      if 'message' in rule:
        description = 'RULE ' + rule['message']
      else:
        description = 'RULE %s: %s $source' % (self.name, name)
      rule_name = self.WriteNewNinjaRule(name, args, description)

      # TODO: if the command references the outputs directly, we should
      # simplify it to just use $out.

      # Rules can potentially make use of some special variables which
      # must vary per source file.
      # Compute the list of variables we'll need to provide.
      special_locals = ('source', 'root', 'ext', 'name')
      needed_variables = set(['source'])
      for argument in args:
        for var in special_locals:
          if '$' + var in argument:
            needed_variables.add(var)

      # For each source file, write an edge that generates all the outputs.
      for source in rule.get('rule_sources', []):
        basename = os.path.basename(source)
        root, ext = os.path.splitext(basename)
        source = self.GypPathToNinja(source)

        outputs = []
        for output in rule['outputs']:
          outputs.append(output.replace('$root', root))

        extra_bindings = []
        for var in needed_variables:
          if var == 'root':
            extra_bindings.append(('root', root))
          elif var == 'source':
            extra_bindings.append(('source', source))
          elif var == 'ext':
            extra_bindings.append(('ext', ext))
          elif var == 'name':
            extra_bindings.append(('name', basename))
          else:
            assert var == None, repr(var)

        inputs = map(self.GypPathToNinja, rule.get('inputs', []))
        self.ninja.build(outputs, rule_name, source,
                         implicit=inputs,
                         order_only=prebuild,
                         variables=extra_bindings)

        if int(rule.get('process_outputs_as_sources', False)):
          extra_sources += outputs

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
      output = self.GypPathToUniqueOutput(filename + '.o', qualified=True)
      self.ninja.build(output, command, input,
                       order_only=predepends)
      outputs.append(output)
    self.ninja.newline()
    return outputs

  def WriteTarget(self, spec, config, final_deps):
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
            if 'lastchange.stamp' not in input:
              implicit_deps.add(input)
        final_deps.extend(list(extra_deps))
    command_map = {
      'executable':      'link',
      'static_library':  'alink',
      'loadable_module': 'solink',
      'shared_library':  'solink',
      'none':            'stamp',
    }
    command = command_map[spec['type']]

    if output_uses_linker:
      self.WriteVariableList('ldflags',
                             gyp.common.uniquer(config.get('ldflags', [])))
      self.WriteVariableList('libs',
                             gyp.common.uniquer(spec.get('libraries', [])))

    extra_bindings = []
    if command == 'solink':
      extra_bindings.append(('soname', os.path.split(output)[1]))

    self.ninja.build(output, command, final_deps,
                     implicit=list(implicit_deps),
                     variables=extra_bindings)

    # Write a short name to build this target.  This benefits both the
    # "build chrome" case as well as the gyp tests, which expect to be
    # able to run actions and build libraries by their short name.
    self.ninja.build(self.name, 'phony', output)

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
    elif spec['type'] == 'settings':
      return None
    else:
      raise 'Unhandled output type', spec['type']

  def ComputeOutput(self, spec):
    """Compute the path for the final output of the spec."""

    filename = self.ComputeOutputFileName(spec)

    if 'product_dir' in spec:
      path = os.path.join(spec['product_dir'], filename)
      return path

    # Executables and loadable modules go into the output root,
    # libraries go into shared library dir, and everything else
    # goes into the normal place.
    if spec['type'] in ('executable', 'loadable_module'):
      return os.path.join('$b', filename)
    elif spec['type'] == 'shared_library':
      return os.path.join('$b/lib', filename)
    else:
      return self.GypPathToUniqueOutput(filename)

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
    rule_name = ('%s.%s' % (self.name, name)).replace(' ', '_')

    cd = ''
    args = args[:]
    if self.base_dir:
      # gyp dictates that commands are run from the base directory.
      # cd into the directory before running, and adjust all paths in
      # the arguments point to the proper locations.
      cd = 'cd %s; ' % self.base_dir
      cdup = '../' * len(self.base_dir.split('/'))
      for i, arg in enumerate(args):
        arg = arg.replace('$b', cdup + '$b')
        arg = arg.replace('$source', cdup + '$source')
        args[i] = arg

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

  master_ninja = OpenOutput(os.path.join(options.toplevel_dir, builddir,
                                         'build.ninja'))
  master_ninja.write(NINJA_BASE % {
      'builddir': builddir,
      'cc': os.environ.get('CC', 'gcc'),
      'cxx': os.environ.get('CXX', 'g++'),
      })

  all_targets = set()
  for build_file in params['build_files']:
    for target in gyp.common.AllTargets(target_list, target_dicts, build_file):
      all_targets.add(target)
  all_outputs = set()

  subninjas = set()
  target_outputs = {}
  for qualified_target in target_list:
    # qualified_target is like: third_party/icu/icu.gyp:icui18n#target
    build_file, target, _ = gyp.common.ParseQualifiedTarget(qualified_target)

    # TODO: what is options.depth and how is it different than
    # options.toplevel_dir?
    build_file = gyp.common.RelativePath(build_file, options.depth)

    base_path = os.path.dirname(build_file)
    output_file = os.path.join(builddir, 'obj', base_path, target + '.ninja')
    spec = target_dicts[qualified_target]
    config = spec['configurations'][config_name]

    writer = NinjaWriter(target_outputs, base_path,
                         OpenOutput(os.path.join(options.toplevel_dir,
                                                 output_file)))
    subninjas.add(output_file)

    output = writer.WriteSpec(spec, config)
    if output:
      linkable = spec['type'] in ('static_library', 'shared_library')
      target_outputs[qualified_target] = (output, linkable)

      if qualified_target in all_targets:
        all_outputs.add(output)

  for ninja in subninjas:
    print >>master_ninja, 'subninja', ninja

  if all_outputs:
    print >>master_ninja, 'build all: phony ||' + ' '.join(all_outputs)

  master_ninja.close()
