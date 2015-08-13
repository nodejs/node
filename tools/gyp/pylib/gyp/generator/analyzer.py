# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script is intended for use as a GYP_GENERATOR. It takes as input (by way of
the generator flag config_path) the path of a json file that dictates the files
and targets to search for. The following keys are supported:
files: list of paths (relative) of the files to search for.
targets: list of targets to search for. The target names are unqualified.

The following is output:
error: only supplied if there is an error.
targets: the set of targets passed in via targets that either directly or
  indirectly depend upon the set of paths supplied in files.
build_targets: minimal set of targets that directly depend on the changed
  files and need to be built. The expectation is this set of targets is passed
  into a build step.
status: outputs one of three values: none of the supplied files were found,
  one of the include files changed so that it should be assumed everything
  changed (in this case targets and build_targets are not output) or at
  least one file was found.
invalid_targets: list of supplied targets thare were not found.

If the generator flag analyzer_output_path is specified, output is written
there. Otherwise output is written to stdout.
"""

import gyp.common
import gyp.ninja_syntax as ninja_syntax
import json
import os
import posixpath
import sys

debug = False

found_dependency_string = 'Found dependency'
no_dependency_string = 'No dependencies'
# Status when it should be assumed that everything has changed.
all_changed_string = 'Found dependency (all)'

# MatchStatus is used indicate if and how a target depends upon the supplied
# sources.
# The target's sources contain one of the supplied paths.
MATCH_STATUS_MATCHES = 1
# The target has a dependency on another target that contains one of the
# supplied paths.
MATCH_STATUS_MATCHES_BY_DEPENDENCY = 2
# The target's sources weren't in the supplied paths and none of the target's
# dependencies depend upon a target that matched.
MATCH_STATUS_DOESNT_MATCH = 3
# The target doesn't contain the source, but the dependent targets have not yet
# been visited to determine a more specific status yet.
MATCH_STATUS_TBD = 4

generator_supports_multiple_toolsets = gyp.common.CrossCompileRequested()

generator_wants_static_library_dependencies_adjusted = False

generator_default_variables = {
}
for dirname in ['INTERMEDIATE_DIR', 'SHARED_INTERMEDIATE_DIR', 'PRODUCT_DIR',
                'LIB_DIR', 'SHARED_LIB_DIR']:
  generator_default_variables[dirname] = '!!!'

for unused in ['RULE_INPUT_PATH', 'RULE_INPUT_ROOT', 'RULE_INPUT_NAME',
               'RULE_INPUT_DIRNAME', 'RULE_INPUT_EXT',
               'EXECUTABLE_PREFIX', 'EXECUTABLE_SUFFIX',
               'STATIC_LIB_PREFIX', 'STATIC_LIB_SUFFIX',
               'SHARED_LIB_PREFIX', 'SHARED_LIB_SUFFIX',
               'CONFIGURATION_NAME']:
  generator_default_variables[unused] = ''


def _ToGypPath(path):
  """Converts a path to the format used by gyp."""
  if os.sep == '\\' and os.altsep == '/':
    return path.replace('\\', '/')
  return path


def _ResolveParent(path, base_path_components):
  """Resolves |path|, which starts with at least one '../'. Returns an empty
  string if the path shouldn't be considered. See _AddSources() for a
  description of |base_path_components|."""
  depth = 0
  while path.startswith('../'):
    depth += 1
    path = path[3:]
  # Relative includes may go outside the source tree. For example, an action may
  # have inputs in /usr/include, which are not in the source tree.
  if depth > len(base_path_components):
    return ''
  if depth == len(base_path_components):
    return path
  return '/'.join(base_path_components[0:len(base_path_components) - depth]) + \
      '/' + path


def _AddSources(sources, base_path, base_path_components, result):
  """Extracts valid sources from |sources| and adds them to |result|. Each
  source file is relative to |base_path|, but may contain '..'. To make
  resolving '..' easier |base_path_components| contains each of the
  directories in |base_path|. Additionally each source may contain variables.
  Such sources are ignored as it is assumed dependencies on them are expressed
  and tracked in some other means."""
  # NOTE: gyp paths are always posix style.
  for source in sources:
    if not len(source) or source.startswith('!!!') or source.startswith('$'):
      continue
    # variable expansion may lead to //.
    org_source = source
    source = source[0] + source[1:].replace('//', '/')
    if source.startswith('../'):
      source = _ResolveParent(source, base_path_components)
      if len(source):
        result.append(source)
      continue
    result.append(base_path + source)
    if debug:
      print 'AddSource', org_source, result[len(result) - 1]


def _ExtractSourcesFromAction(action, base_path, base_path_components,
                              results):
  if 'inputs' in action:
    _AddSources(action['inputs'], base_path, base_path_components, results)


def _ToLocalPath(toplevel_dir, path):
  """Converts |path| to a path relative to |toplevel_dir|."""
  if path == toplevel_dir:
    return ''
  if path.startswith(toplevel_dir + '/'):
    return path[len(toplevel_dir) + len('/'):]
  return path


def _ExtractSources(target, target_dict, toplevel_dir):
  # |target| is either absolute or relative and in the format of the OS. Gyp
  # source paths are always posix. Convert |target| to a posix path relative to
  # |toplevel_dir_|. This is done to make it easy to build source paths.
  base_path = posixpath.dirname(_ToLocalPath(toplevel_dir, _ToGypPath(target)))
  base_path_components = base_path.split('/')

  # Add a trailing '/' so that _AddSources() can easily build paths.
  if len(base_path):
    base_path += '/'

  if debug:
    print 'ExtractSources', target, base_path

  results = []
  if 'sources' in target_dict:
    _AddSources(target_dict['sources'], base_path, base_path_components,
                results)
  # Include the inputs from any actions. Any changes to these affect the
  # resulting output.
  if 'actions' in target_dict:
    for action in target_dict['actions']:
      _ExtractSourcesFromAction(action, base_path, base_path_components,
                                results)
  if 'rules' in target_dict:
    for rule in target_dict['rules']:
      _ExtractSourcesFromAction(rule, base_path, base_path_components, results)

  return results


class Target(object):
  """Holds information about a particular target:
  deps: set of Targets this Target depends upon. This is not recursive, only the
    direct dependent Targets.
  match_status: one of the MatchStatus values.
  back_deps: set of Targets that have a dependency on this Target.
  visited: used during iteration to indicate whether we've visited this target.
    This is used for two iterations, once in building the set of Targets and
    again in _GetBuildTargets().
  name: fully qualified name of the target.
  requires_build: True if the target type is such that it needs to be built.
    See _DoesTargetTypeRequireBuild for details.
  added_to_compile_targets: used when determining if the target was added to the
    set of targets that needs to be built.
  in_roots: true if this target is a descendant of one of the root nodes.
  is_executable: true if the type of target is executable.
  is_static_library: true if the type of target is static_library.
  is_or_has_linked_ancestor: true if the target does a link (eg executable), or
    if there is a target in back_deps that does a link."""
  def __init__(self, name):
    self.deps = set()
    self.match_status = MATCH_STATUS_TBD
    self.back_deps = set()
    self.name = name
    # TODO(sky): I don't like hanging this off Target. This state is specific
    # to certain functions and should be isolated there.
    self.visited = False
    self.requires_build = False
    self.added_to_compile_targets = False
    self.in_roots = False
    self.is_executable = False
    self.is_static_library = False
    self.is_or_has_linked_ancestor = False


class Config(object):
  """Details what we're looking for
  files: set of files to search for
  targets: see file description for details."""
  def __init__(self):
    self.files = []
    self.targets = set()

  def Init(self, params):
    """Initializes Config. This is a separate method as it raises an exception
    if there is a parse error."""
    generator_flags = params.get('generator_flags', {})
    config_path = generator_flags.get('config_path', None)
    if not config_path:
      return
    try:
      f = open(config_path, 'r')
      config = json.load(f)
      f.close()
    except IOError:
      raise Exception('Unable to open file ' + config_path)
    except ValueError as e:
      raise Exception('Unable to parse config file ' + config_path + str(e))
    if not isinstance(config, dict):
      raise Exception('config_path must be a JSON file containing a dictionary')
    self.files = config.get('files', [])
    self.targets = set(config.get('targets', []))


def _WasBuildFileModified(build_file, data, files, toplevel_dir):
  """Returns true if the build file |build_file| is either in |files| or
  one of the files included by |build_file| is in |files|. |toplevel_dir| is
  the root of the source tree."""
  if _ToLocalPath(toplevel_dir, _ToGypPath(build_file)) in files:
    if debug:
      print 'gyp file modified', build_file
    return True

  # First element of included_files is the file itself.
  if len(data[build_file]['included_files']) <= 1:
    return False

  for include_file in data[build_file]['included_files'][1:]:
    # |included_files| are relative to the directory of the |build_file|.
    rel_include_file = \
        _ToGypPath(gyp.common.UnrelativePath(include_file, build_file))
    if _ToLocalPath(toplevel_dir, rel_include_file) in files:
      if debug:
        print 'included gyp file modified, gyp_file=', build_file, \
            'included file=', rel_include_file
      return True
  return False


def _GetOrCreateTargetByName(targets, target_name):
  """Creates or returns the Target at targets[target_name]. If there is no
  Target for |target_name| one is created. Returns a tuple of whether a new
  Target was created and the Target."""
  if target_name in targets:
    return False, targets[target_name]
  target = Target(target_name)
  targets[target_name] = target
  return True, target


def _DoesTargetTypeRequireBuild(target_dict):
  """Returns true if the target type is such that it needs to be built."""
  # If a 'none' target has rules or actions we assume it requires a build.
  return bool(target_dict['type'] != 'none' or
              target_dict.get('actions') or target_dict.get('rules'))


def _GenerateTargets(data, target_list, target_dicts, toplevel_dir, files,
                     build_files):
  """Returns a tuple of the following:
  . A dictionary mapping from fully qualified name to Target.
  . A list of the targets that have a source file in |files|.
  . Set of root Targets reachable from the the files |build_files|.
  This sets the |match_status| of the targets that contain any of the source
  files in |files| to MATCH_STATUS_MATCHES.
  |toplevel_dir| is the root of the source tree."""
  # Maps from target name to Target.
  targets = {}

  # Targets that matched.
  matching_targets = []

  # Queue of targets to visit.
  targets_to_visit = target_list[:]

  # Maps from build file to a boolean indicating whether the build file is in
  # |files|.
  build_file_in_files = {}

  # Root targets across all files.
  roots = set()

  # Set of Targets in |build_files|.
  build_file_targets = set()

  while len(targets_to_visit) > 0:
    target_name = targets_to_visit.pop()
    created_target, target = _GetOrCreateTargetByName(targets, target_name)
    if created_target:
      roots.add(target)
    elif target.visited:
      continue

    target.visited = True
    target.requires_build = _DoesTargetTypeRequireBuild(
        target_dicts[target_name])
    target_type = target_dicts[target_name]['type']
    target.is_executable = target_type == 'executable'
    target.is_static_library = target_type == 'static_library'
    target.is_or_has_linked_ancestor = (target_type == 'executable' or
                                        target_type == 'shared_library')

    build_file = gyp.common.ParseQualifiedTarget(target_name)[0]
    if not build_file in build_file_in_files:
      build_file_in_files[build_file] = \
          _WasBuildFileModified(build_file, data, files, toplevel_dir)

    if build_file in build_files:
      build_file_targets.add(target)

    # If a build file (or any of its included files) is modified we assume all
    # targets in the file are modified.
    if build_file_in_files[build_file]:
      print 'matching target from modified build file', target_name
      target.match_status = MATCH_STATUS_MATCHES
      matching_targets.append(target)
    else:
      sources = _ExtractSources(target_name, target_dicts[target_name],
                                toplevel_dir)
      for source in sources:
        if source in files:
          print 'target', target_name, 'matches', source
          target.match_status = MATCH_STATUS_MATCHES
          matching_targets.append(target)
          break

    # Add dependencies to visit as well as updating back pointers for deps.
    for dep in target_dicts[target_name].get('dependencies', []):
      targets_to_visit.append(dep)

      created_dep_target, dep_target = _GetOrCreateTargetByName(targets, dep)
      if not created_dep_target:
        roots.discard(dep_target)

      target.deps.add(dep_target)
      dep_target.back_deps.add(target)

  return targets, matching_targets, roots & build_file_targets


def _GetUnqualifiedToTargetMapping(all_targets, to_find):
  """Returns a mapping (dictionary) from unqualified name to Target for all the
  Targets in |to_find|."""
  result = {}
  if not to_find:
    return result
  to_find = set(to_find)
  for target_name in all_targets.keys():
    extracted = gyp.common.ParseQualifiedTarget(target_name)
    if len(extracted) > 1 and extracted[1] in to_find:
      to_find.remove(extracted[1])
      result[extracted[1]] = all_targets[target_name]
      if not to_find:
        return result
  return result


def _DoesTargetDependOn(target):
  """Returns true if |target| or any of its dependencies matches the supplied
  set of paths. This updates |matches| of the Targets as it recurses.
  target: the Target to look for."""
  if target.match_status == MATCH_STATUS_DOESNT_MATCH:
    return False
  if target.match_status == MATCH_STATUS_MATCHES or \
      target.match_status == MATCH_STATUS_MATCHES_BY_DEPENDENCY:
    return True
  for dep in target.deps:
    if _DoesTargetDependOn(dep):
      target.match_status = MATCH_STATUS_MATCHES_BY_DEPENDENCY
      print '\t', target.name, 'matches by dep', dep.name
      return True
  target.match_status = MATCH_STATUS_DOESNT_MATCH
  return False


def _GetTargetsDependingOn(possible_targets):
  """Returns the list of Targets in |possible_targets| that depend (either
  directly on indirectly) on the matched targets.
  possible_targets: targets to search from."""
  found = []
  print 'Targets that matched by dependency:'
  for target in possible_targets:
    if _DoesTargetDependOn(target):
      found.append(target)
  return found


def _AddBuildTargets(target, roots, add_if_no_ancestor, result):
  """Recurses through all targets that depend on |target|, adding all targets
  that need to be built (and are in |roots|) to |result|.
  roots: set of root targets.
  add_if_no_ancestor: If true and there are no ancestors of |target| then add
  |target| to |result|. |target| must still be in |roots|.
  result: targets that need to be built are added here."""
  if target.visited:
    return

  target.visited = True
  target.in_roots = not target.back_deps and target in roots

  for back_dep_target in target.back_deps:
    _AddBuildTargets(back_dep_target, roots, False, result)
    target.added_to_compile_targets |= back_dep_target.added_to_compile_targets
    target.in_roots |= back_dep_target.in_roots
    target.is_or_has_linked_ancestor |= (
      back_dep_target.is_or_has_linked_ancestor)

  # Always add 'executable' targets. Even though they may be built by other
  # targets that depend upon them it makes detection of what is going to be
  # built easier.
  # And always add static_libraries that have no dependencies on them from
  # linkables. This is necessary as the other dependencies on them may be
  # static libraries themselves, which are not compile time dependencies.
  if target.in_roots and \
        (target.is_executable or
         (not target.added_to_compile_targets and
          (add_if_no_ancestor or target.requires_build)) or
         (target.is_static_library and add_if_no_ancestor and
          not target.is_or_has_linked_ancestor)):
    print '\t\tadding to build targets', target.name, 'executable', \
           target.is_executable, 'added_to_compile_targets', \
           target.added_to_compile_targets, 'add_if_no_ancestor', \
           add_if_no_ancestor, 'requires_build', target.requires_build, \
           'is_static_library', target.is_static_library, \
           'is_or_has_linked_ancestor', target.is_or_has_linked_ancestor
    result.add(target)
    target.added_to_compile_targets = True


def _GetBuildTargets(matching_targets, roots):
  """Returns the set of Targets that require a build.
  matching_targets: targets that changed and need to be built.
  roots: set of root targets in the build files to search from."""
  result = set()
  for target in matching_targets:
    print '\tfinding build targets for match', target.name
    _AddBuildTargets(target, roots, True, result)
  return result


def _WriteOutput(params, **values):
  """Writes the output, either to stdout or a file is specified."""
  if 'error' in values:
    print 'Error:', values['error']
  if 'status' in values:
    print values['status']
  if 'targets' in values:
    values['targets'].sort()
    print 'Supplied targets that depend on changed files:'
    for target in values['targets']:
      print '\t', target
  if 'invalid_targets' in values:
    values['invalid_targets'].sort()
    print 'The following targets were not found:'
    for target in values['invalid_targets']:
      print '\t', target
  if 'build_targets' in values:
    values['build_targets'].sort()
    print 'Targets that require a build:'
    for target in values['build_targets']:
      print '\t', target

  output_path = params.get('generator_flags', {}).get(
      'analyzer_output_path', None)
  if not output_path:
    print json.dumps(values)
    return
  try:
    f = open(output_path, 'w')
    f.write(json.dumps(values) + '\n')
    f.close()
  except IOError as e:
    print 'Error writing to output file', output_path, str(e)


def _WasGypIncludeFileModified(params, files):
  """Returns true if one of the files in |files| is in the set of included
  files."""
  if params['options'].includes:
    for include in params['options'].includes:
      if _ToGypPath(include) in files:
        print 'Include file modified, assuming all changed', include
        return True
  return False


def _NamesNotIn(names, mapping):
  """Returns a list of the values in |names| that are not in |mapping|."""
  return [name for name in names if name not in mapping]


def _LookupTargets(names, mapping):
  """Returns a list of the mapping[name] for each value in |names| that is in
  |mapping|."""
  return [mapping[name] for name in names if name in mapping]


def CalculateVariables(default_variables, params):
  """Calculate additional variables for use in the build (called by gyp)."""
  flavor = gyp.common.GetFlavor(params)
  if flavor == 'mac':
    default_variables.setdefault('OS', 'mac')
  elif flavor == 'win':
    default_variables.setdefault('OS', 'win')
    # Copy additional generator configuration data from VS, which is shared
    # by the Windows Ninja generator.
    import gyp.generator.msvs as msvs_generator
    generator_additional_non_configuration_keys = getattr(msvs_generator,
        'generator_additional_non_configuration_keys', [])
    generator_additional_path_sections = getattr(msvs_generator,
        'generator_additional_path_sections', [])

    gyp.msvs_emulation.CalculateCommonVariables(default_variables, params)
  else:
    operating_system = flavor
    if flavor == 'android':
      operating_system = 'linux'  # Keep this legacy behavior for now.
    default_variables.setdefault('OS', operating_system)


def GenerateOutput(target_list, target_dicts, data, params):
  """Called by gyp as the final stage. Outputs results."""
  config = Config()
  try:
    config.Init(params)
    if not config.files:
      raise Exception('Must specify files to analyze via config_path generator '
                      'flag')

    toplevel_dir = _ToGypPath(os.path.abspath(params['options'].toplevel_dir))
    if debug:
      print 'toplevel_dir', toplevel_dir

    if _WasGypIncludeFileModified(params, config.files):
      result_dict = { 'status': all_changed_string,
                      'targets': list(config.targets) }
      _WriteOutput(params, **result_dict)
      return

    all_targets, matching_targets, roots = _GenerateTargets(
      data, target_list, target_dicts, toplevel_dir, frozenset(config.files),
      params['build_files'])

    print 'roots:'
    for root in roots:
      print '\t', root.name

    unqualified_mapping = _GetUnqualifiedToTargetMapping(all_targets,
                                                         config.targets)
    invalid_targets = None
    if len(unqualified_mapping) != len(config.targets):
      invalid_targets = _NamesNotIn(config.targets, unqualified_mapping)

    if matching_targets:
      search_targets = _LookupTargets(config.targets, unqualified_mapping)
      print 'supplied targets'
      for target in config.targets:
        print '\t', target
      print 'expanded supplied targets'
      for target in search_targets:
        print '\t', target.name
      matched_search_targets = _GetTargetsDependingOn(search_targets)
      print 'raw matched search targets:'
      for target in matched_search_targets:
        print '\t', target.name
      # Reset the visited status for _GetBuildTargets.
      for target in all_targets.itervalues():
        target.visited = False
      print 'Finding build targets'
      build_targets = _GetBuildTargets(matching_targets, roots)
      matched_search_targets = [gyp.common.ParseQualifiedTarget(target.name)[1]
                                for target in matched_search_targets]
      build_targets = [gyp.common.ParseQualifiedTarget(target.name)[1]
                       for target in build_targets]
    else:
      matched_search_targets = []
      build_targets = []

    result_dict = { 'targets': matched_search_targets,
                    'status': found_dependency_string if matching_targets else
                              no_dependency_string,
                    'build_targets': build_targets}
    if invalid_targets:
      result_dict['invalid_targets'] = invalid_targets
    _WriteOutput(params, **result_dict)

  except Exception as e:
    _WriteOutput(params, error=str(e))
