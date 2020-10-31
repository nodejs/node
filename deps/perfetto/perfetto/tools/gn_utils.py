# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# A collection of utilities for extracting build rule information from GN
# projects.

from __future__ import print_function
import errno
import filecmp
import json
import os
import re
import shutil
import subprocess
import sys
from compat import iteritems

BUILDFLAGS_TARGET = '//gn:gen_buildflags'
TARGET_TOOLCHAIN = '//gn/standalone/toolchain:gcc_like_host'
HOST_TOOLCHAIN = '//gn/standalone/toolchain:gcc_like_host'
LINKER_UNIT_TYPES = ('executable', 'shared_library', 'static_library')


def _check_command_output(cmd, cwd):
  try:
    output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, cwd=cwd)
  except subprocess.CalledProcessError as e:
    print(
        'Command "{}" failed in {}:'.format(' '.join(cmd), cwd),
        file=sys.stderr)
    print(e.output.decode(), file=sys.stderr)
    sys.exit(1)
  else:
    return output.decode()


def repo_root():
  """Returns an absolute path to the repository root."""
  return os.path.join(
      os.path.realpath(os.path.dirname(__file__)), os.path.pardir)


def _tool_path(name):
  return os.path.join(repo_root(), 'tools', name)


def prepare_out_directory(gn_args, name, root=repo_root()):
  """Creates the JSON build description by running GN.

    Returns (path, desc) where |path| is the location of the output directory
    and |desc| is the JSON build description.
    """
  out = os.path.join(root, 'out', name)
  try:
    os.makedirs(out)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise
  _check_command_output([_tool_path('gn'), 'gen', out,
                         '--args=%s' % gn_args],
                        cwd=repo_root())
  return out


def load_build_description(out):
  """Creates the JSON build description by running GN."""
  desc = _check_command_output([
      _tool_path('gn'), 'desc', out, '--format=json', '--all-toolchains', '//*'
  ],
                               cwd=repo_root())
  return json.loads(desc)


def create_build_description(gn_args, root=repo_root()):
  """Prepares a GN out directory and loads the build description from it.

    The temporary out directory is automatically deleted.
    """
  out = prepare_out_directory(gn_args, 'tmp.gn_utils', root=root)
  try:
    return load_build_description(out)
  finally:
    shutil.rmtree(out)


def build_targets(out, targets, quiet=False):
  """Runs ninja to build a list of GN targets in the given out directory.

    Compiling these targets is required so that we can include any generated
    source files in the amalgamated result.
    """
  targets = [t.replace('//', '') for t in targets]
  with open(os.devnull, 'w') as devnull:
    stdout = devnull if quiet else None
    subprocess.check_call(
        [_tool_path('ninja')] + targets, cwd=out, stdout=stdout)


def compute_source_dependencies(out):
  """For each source file, computes a set of headers it depends on."""
  ninja_deps = _check_command_output([_tool_path('ninja'), '-t', 'deps'],
                                     cwd=out)
  deps = {}
  current_source = None
  for line in ninja_deps.split('\n'):
    filename = os.path.relpath(os.path.join(out, line.strip()), repo_root())
    if not line or line[0] != ' ':
      current_source = None
      continue
    elif not current_source:
      # We're assuming the source file is always listed before the
      # headers.
      assert os.path.splitext(line)[1] in ['.c', '.cc', '.cpp', '.S']
      current_source = filename
      deps[current_source] = []
    else:
      assert current_source
      deps[current_source].append(filename)
  return deps


def label_to_path(label):
  """Turn a GN output label (e.g., //some_dir/file.cc) into a path."""
  assert label.startswith('//')
  return label[2:]


def label_without_toolchain(label):
  """Strips the toolchain from a GN label.

    Return a GN label (e.g //buildtools:protobuf(//gn/standalone/toolchain:
    gcc_like_host) without the parenthesised toolchain part.
    """
  return label.split('(')[0]


def label_to_target_name_with_path(label):
  """
  Turn a GN label into a target name involving the full path.
  e.g., //src/perfetto:tests -> src_perfetto_tests
  """
  name = re.sub(r'^//:?', '', label)
  name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
  return name


def gen_buildflags(gn_args, target_file):
  """Generates the perfetto_build_flags.h for the given config.

    target_file: the path, relative to the repo root, where the generated
        buildflag header will be copied into.
    """
  tmp_out = prepare_out_directory(gn_args, 'tmp.gen_buildflags')
  build_targets(tmp_out, [BUILDFLAGS_TARGET], quiet=True)
  src = os.path.join(tmp_out, 'gen', 'build_config', 'perfetto_build_flags.h')
  shutil.copy(src, os.path.join(repo_root(), target_file))
  shutil.rmtree(tmp_out)


def check_or_commit_generated_files(tmp_files, check):
  """Checks that gen files are unchanged or renames them to the final location

    Takes in input a list of 'xxx.swp' files that have been written.
    If check == False, it renames xxx.swp -> xxx.
    If check == True, it just checks that the contents of 'xxx.swp' == 'xxx'.
    Returns 0 if no diff was detected, 1 otherwise (to be used as exit code).
    """
  res = 0
  for tmp_file in tmp_files:
    assert (tmp_file.endswith('.swp'))
    target_file = os.path.relpath(tmp_file[:-4])
    if check:
      if not filecmp.cmp(tmp_file, target_file):
        sys.stderr.write('%s needs to be regenerated\n' % target_file)
        res = 1
      os.unlink(tmp_file)
    else:
      os.rename(tmp_file, target_file)
  return res


class GnParser(object):
  """A parser with some cleverness for GN json desc files

    The main goals of this parser are:
    1) Deal with the fact that other build systems don't have an equivalent
       notion to GN's source_set. Conversely to Bazel's and Soong's filegroups,
       GN source_sets expect that dependencies, cflags and other source_set
       properties propagate up to the linker unit (static_library, executable or
       shared_library). This parser simulates the same behavior: when a
       source_set is encountered, some of its variables (cflags and such) are
       copied up to the dependent targets. This is to allow gen_xxx to create
       one filegroup for each source_set and then squash all the other flags
       onto the linker unit.
    2) Detect and special-case protobuf targets, figuring out the protoc-plugin
       being used.
    """

  class Target(object):
    """Reperesents A GN target.

        Maked properties are propagated up the dependency chain when a
        source_set dependency is encountered.
        """

    def __init__(self, name, type):
      self.name = name  # e.g. //src/ipc:ipc

      VALID_TYPES = ('static_library', 'shared_library', 'executable', 'group',
                     'action', 'source_set', 'proto_library')
      assert (type in VALID_TYPES)
      self.type = type
      self.testonly = False
      self.toolchain = None

      # Only set when type == proto_library.
      # This is typically: 'proto', 'protozero', 'ipc'.
      self.proto_plugin = None

      self.sources = set()

      # These are valid only for type == 'action'
      self.inputs = set()
      self.outputs = set()
      self.script = None
      self.args = []

      # These variables are propagated up when encountering a dependency
      # on a source_set target.
      self.cflags = set()
      self.defines = set()
      self.deps = set()
      self.libs = set()
      self.include_dirs = set()
      self.ldflags = set()
      self.source_set_deps = set()  # Transitive set of source_set deps.
      self.proto_deps = set()  # Transitive set of protobuf deps.

      # Deps on //gn:xxx have this flag set to True. These dependencies
      # are special because they pull third_party code from buildtools/.
      # We don't want to keep recursing into //buildtools in generators,
      # this flag is used to stop the recursion and create an empty
      # placeholder target once we hit //gn:protoc or similar.
      self.is_third_party_dep_ = False

    def __lt__(self, other):
      if isinstance(other, self.__class__):
        return self.name < other.name
      raise TypeError(
          '\'<\' not supported between instances of \'%s\' and \'%s\'' %
          (type(self).__name__, type(other).__name__))

    def __repr__(self):
      return json.dumps({
          k: (list(sorted(v)) if isinstance(v, set) else v)
          for (k, v) in iteritems(self.__dict__)
      },
                        indent=4,
                        sort_keys=True)

    def update(self, other):
      for key in ('cflags', 'defines', 'deps', 'include_dirs', 'ldflags',
                  'source_set_deps', 'proto_deps', 'libs'):
        self.__dict__[key].update(other.__dict__.get(key, []))

  def __init__(self, gn_desc):
    self.gn_desc_ = gn_desc
    self.all_targets = {}
    self.linker_units = {}  # Executables, shared or static libraries.
    self.source_sets = {}
    self.actions = {}
    self.proto_libs = {}

  def get_target(self, gn_target_name):
    """Returns a Target object from the fully qualified GN target name.

        It bubbles up variables from source_set dependencies as described in the
        class-level comments.
        """
    target = self.all_targets.get(gn_target_name)
    if target is not None:
      return target  # Target already processed.

    desc = self.gn_desc_[gn_target_name]
    target = GnParser.Target(gn_target_name, desc['type'])
    target.testonly = desc.get('testonly', False)
    target.toolchain = desc.get('toolchain', None)
    self.all_targets[gn_target_name] = target

    # We should never have GN targets directly depend on buidtools. They
    # should hop via //gn:xxx, so we can give generators an opportunity to
    # override them.
    assert (not gn_target_name.startswith('//buildtools'))

    # Don't descend further into third_party targets. Genrators are supposed
    # to either ignore them or route to other externally-provided targets.
    if gn_target_name.startswith('//gn'):
      target.is_third_party_dep_ = True
      return target

    proto_target_type, proto_desc = self.get_proto_target_type_(target)
    if proto_target_type is not None:
      self.proto_libs[target.name] = target
      target.type = 'proto_library'
      target.proto_plugin = proto_target_type
      target.sources.update(proto_desc.get('sources', []))
      assert (all(x.endswith('.proto') for x in target.sources))
    elif target.type == 'source_set':
      self.source_sets[gn_target_name] = target
      target.sources.update(desc.get('sources', []))
    elif target.type in LINKER_UNIT_TYPES:
      self.linker_units[gn_target_name] = target
      target.sources.update(desc.get('sources', []))
    elif target.type == 'action':
      self.actions[gn_target_name] = target
      target.inputs.update(desc['inputs'])
      outs = [re.sub('^//out/.+?/gen/', '', x) for x in desc['outputs']]
      target.outputs.update(outs)
      target.script = desc['script']
      # Args are typically relative to the root build dir (../../xxx)
      # because root build dir is typically out/xxx/).
      target.args = [re.sub('^../../', '//', x) for x in desc['args']]

    target.cflags.update(desc.get('cflags', []) + desc.get('cflags_cc', []))
    target.libs.update(desc.get('libs', []))
    target.ldflags.update(desc.get('ldflags', []))
    target.defines.update(desc.get('defines', []))
    target.include_dirs.update(desc.get('include_dirs', []))

    # Recurse in dependencies.
    for dep_name in desc.get('deps', []):
      dep = self.get_target(dep_name)
      if dep.is_third_party_dep_:
        target.deps.add(dep_name)
      elif dep.type == 'proto_library':
        target.proto_deps.add(dep_name)
        target.proto_deps.update(dep.proto_deps)  # Bubble up deps.
      elif dep.type == 'source_set':
        target.source_set_deps.add(dep_name)
        target.update(dep)  # Bubble up source set's cflags/ldflags etc.
      elif dep.type == 'group':
        target.update(dep)  # Bubble up groups's cflags/ldflags etc.
      elif dep.type == 'action':
        if proto_target_type is None:
          target.deps.add(dep_name)
      elif dep.type in LINKER_UNIT_TYPES:
        target.deps.add(dep_name)

    return target

  def get_proto_target_type_(self, target):
    """ Checks if the target is a proto library and return the plugin.

        Returns:
            (None, None): if the target is not a proto library.
            (plugin, gen_desc) where |plugin| is 'proto' in the default (lite)
            case or 'protozero' or 'ipc'; |gen_desc| is the GN json descriptor
            of the _gen target (the one with .proto sources).
        """
    parts = target.name.split('(', 1)
    name = parts[0]
    toolchain = '(' + parts[1] if len(parts) > 1 else ''
    gen_desc = self.gn_desc_.get('%s_gen%s' % (name, toolchain))
    if gen_desc is None or gen_desc['type'] != 'action':
      return None, None
    args = gen_desc.get('args', [])
    if '/protoc' not in args[0]:
      return None, None
    plugin = 'proto'
    for arg in (arg for arg in args if arg.startswith('--plugin=')):
      # |arg| at this point looks like:
      #  --plugin=protoc-gen-plugin=gcc_like_host/protozero_plugin
      # or
      #  --plugin=protoc-gen-plugin=protozero_plugin
      plugin = arg.split('=')[-1].split('/')[-1].replace('_plugin', '')
    return plugin, gen_desc
