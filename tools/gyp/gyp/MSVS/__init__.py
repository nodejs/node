from __future__ import print_function

import copy
import json
import os
import subprocess
import sys
import traceback

from gyp.common import memoize

try:
  WindowsError
except NameError:
  # noinspection PyShadowingBuiltins
  WindowsError = OSError

try:
  import winreg
except ImportError:
  try:
    import _winreg as winreg
  except ImportError:
    # Just a mock class to silence static analysers.
    class winreg(object):
      HKEY_LOCAL_MACHINE = ''
      @staticmethod
      def OpenKey(root, key):
        raise NotImplementedError()
      @staticmethod
      def QueryValueEx(key, value):
        raise NotImplementedError()

class Tool(object):
  """Visual Studio tool."""

  def __init__(self, name, attrs=None):
    """Initializes the tool.

    Args:
      name: Tool name.
      attrs: Dict of tool attributes; may be None.
    """
    self._attrs = attrs or {}
    self._attrs['Name'] = name

  def _GetSpecification(self):
    """Creates an element for the tool.

    Returns:
      A new xml.dom.Element for the tool.
    """
    return ['Tool', self._attrs]


class Filter(object):
  """Visual Studio filter - that is, a virtual folder."""

  def __init__(self, name, contents=None):
    """Initializes the folder.

    Args:
      name: Filter (folder) name.
      contents: List of filenames and/or Filter objects contained.
    """
    self.name = name
    self.contents = list(contents or [])


# A dictionary mapping supported target types to extensions.
TARGET_TYPE_EXT = {
  'executable': 'exe',
  'loadable_module': 'dll',
  'shared_library': 'dll',
  'static_library': 'lib',
  'windows_driver': 'sys',
}


def _GetLargePdbShimCcPath():
  """Returns the path of the large_pdb_shim.cc file."""
  this_dir = os.path.dirname(__file__)
  lib_dir = os.path.join(this_dir, '..')
  large_pdb_shim_cc_rel = os.path.join(lib_dir, 'buildtime_helpers', 'large-pdb-shim.cc')
  large_pdb_shim_cc = os.path.abspath(large_pdb_shim_cc_rel)
  return large_pdb_shim_cc


def _DeepCopySomeKeys(in_dict, keys):
  """Performs a partial deep-copy on |in_dict|, only copying the keys in |keys|.

  Arguments:
    in_dict: The dictionary to copy.
    keys: The keys to be copied. If a key is in this list and doesn't exist in
        |in_dict| this is not an error.
  Returns:
    The partially deep-copied dictionary.
  """
  d = {}
  for key in keys:
    if key not in in_dict:
      continue
    d[key] = copy.deepcopy(in_dict[key])
  return d


def _SuffixName(name, suffix):
  """Add a suffix to the end of a target.

  Arguments:
    name: name of the target (foo#target)
    suffix: the suffix to be added
  Returns:
    Target name with suffix added (foo_suffix#target)
  """
  parts = name.rsplit('#', 1)
  parts[0] = '%s_%s' % (parts[0], suffix)
  return '#'.join(parts)


def _ShardName(name, number):
  """Add a shard number to the end of a target.

  Arguments:
    name: name of the target (foo#target)
    number: shard number
  Returns:
    Target name with shard added (foo_1#target)
  """
  return _SuffixName(name, str(number))


def ShardTargets(target_list, target_dicts):
  """Shard some targets apart to work around the linkers limits.

  Arguments:
    target_list: List of target pairs: 'base/base.gyp:base'.
    target_dicts: Dict of target properties keyed on target pair.
  Returns:
    Tuple of the new sharded versions of the inputs.
  """
  # Gather the targets to shard, and how many pieces.
  targets_to_shard = {}
  for t in target_dicts:
    shards = int(target_dicts[t].get('msvs_shard', 0))
    if shards:
      targets_to_shard[t] = shards
  # Shard target_list.
  new_target_list = []
  for t in target_list:
    if t in targets_to_shard:
      for i in range(targets_to_shard[t]):
        new_target_list.append(_ShardName(t, i))
    else:
      new_target_list.append(t)
  # Shard target_dict.
  new_target_dicts = {}
  for t in target_dicts:
    if t in targets_to_shard:
      for i in range(targets_to_shard[t]):
        name = _ShardName(t, i)
        new_target_dicts[name] = copy.copy(target_dicts[t])
        new_target_dicts[name]['target_name'] = _ShardName(
             new_target_dicts[name]['target_name'], i)
        sources = new_target_dicts[name].get('sources', [])
        new_sources = []
        for pos in range(i, len(sources), targets_to_shard[t]):
          new_sources.append(sources[pos])
        new_target_dicts[name]['sources'] = new_sources
    else:
      new_target_dicts[t] = target_dicts[t]
  # Shard dependencies.
  for t in sorted(new_target_dicts):
    for dep_type in ('dependencies', 'dependencies_original'):
      dependencies = copy.copy(new_target_dicts[t].get(dep_type, []))
      new_dependencies = []
      for d in dependencies:
        if d in targets_to_shard:
          for i in range(targets_to_shard[d]):
            new_dependencies.append(_ShardName(d, i))
        else:
          new_dependencies.append(d)
      new_target_dicts[t][dep_type] = new_dependencies

  return new_target_list, new_target_dicts


def _GetPdbPath(target_dict, config_name, gyp_vars):
  """Returns the path to the PDB file that will be generated by a given
  configuration.

  The lookup proceeds as follows:
    - Look for an explicit path in the VCLinkerTool configuration block.
    - Look for an 'msvs_large_pdb_path' variable.
    - Use '<(PRODUCT_DIR)/<(product_name).(exe|dll).pdb' if 'product_name' is
      specified.
    - Use '<(PRODUCT_DIR)/<(target_name).(exe|dll).pdb'.

  Arguments:
    target_dict: The target dictionary to be searched.
    config_name: The name of the configuration of interest.
    gyp_vars: A dictionary of common GYP variables with generator-specific values.
  Returns:
    The path of the corresponding PDB file.
  """
  config = target_dict['configurations'][config_name]
  msvs = config.setdefault('msvs_settings', {})

  linker = msvs.get('VCLinkerTool', {})

  pdb_path = linker.get('ProgramDatabaseFile')
  if pdb_path:
    return pdb_path

  variables = target_dict.get('variables', {})
  pdb_path = variables.get('msvs_large_pdb_path', None)
  if pdb_path:
    return pdb_path


  pdb_base = target_dict.get('product_name', target_dict['target_name'])
  pdb_base = '%s.%s.pdb' % (pdb_base, TARGET_TYPE_EXT[target_dict['type']])
  pdb_path = gyp_vars['PRODUCT_DIR'] + '/' + pdb_base

  return pdb_path


def InsertLargePdbShims(target_list, target_dicts, gyp_vars):
  """Insert a shim target that forces the linker to use 4KB pagesize PDBs.

  This is a workaround for targets with PDBs greater than 1GB in size, the
  limit for the 1KB pagesize PDBs created by the linker by default.

  Arguments:
    target_list: List of target pairs: 'base/base.gyp:base'.
    target_dicts: Dict of target properties keyed on target pair.
    gyp_vars: A dictionary of common GYP variables with generator-specific values.
  Returns:
    Tuple of the shimmed version of the inputs.
  """
  # Determine which targets need shimming.
  targets_to_shim = []
  for t in target_dicts:
    target_dict = target_dicts[t]

    # We only want to shim targets that have msvs_large_pdb enabled.
    if not int(target_dict.get('msvs_large_pdb', 0)):
      continue
    # This is intended for executable, shared_library and loadable_module
    # targets where every configuration is set up to produce a PDB output.
    # If any of these conditions is not true then the shim logic will fail
    # below.
    targets_to_shim.append(t)

  large_pdb_shim_cc = _GetLargePdbShimCcPath()

  for t in targets_to_shim:
    target_dict = target_dicts[t]
    target_name = target_dict.get('target_name')

    base_dict = _DeepCopySomeKeys(target_dict,
          ['configurations', 'default_configuration', 'toolset'])

    # This is the dict for copying the source file (part of the GYP tree)
    # to the intermediate directory of the project. This is necessary because
    # we can't always build a relative path to the shim source file (on Windows
    # GYP and the project may be on different drives), and Ninja hates absolute
    # paths (it ends up generating the .obj and .obj.d alongside the source
    # file, polluting GYPs tree).
    copy_suffix = 'large_pdb_copy'
    copy_target_name = target_name + '_' + copy_suffix
    full_copy_target_name = _SuffixName(t, copy_suffix)
    shim_cc_basename = os.path.basename(large_pdb_shim_cc)
    shim_cc_dir = gyp_vars['SHARED_INTERMEDIATE_DIR'] + '/' + copy_target_name
    shim_cc_path = shim_cc_dir + '/' + shim_cc_basename
    copy_dict = copy.deepcopy(base_dict)
    copy_dict['target_name'] = copy_target_name
    copy_dict['type'] = 'none'
    copy_dict['sources'] = [ large_pdb_shim_cc ]
    copy_dict['copies'] = [{
      'destination': shim_cc_dir,
      'files': [ large_pdb_shim_cc ]
    }]

    # This is the dict for the PDB generating shim target. It depends on the
    # copy target.
    shim_suffix = 'large_pdb_shim'
    shim_target_name = target_name + '_' + shim_suffix
    full_shim_target_name = _SuffixName(t, shim_suffix)
    shim_dict = copy.deepcopy(base_dict)
    shim_dict['target_name'] = shim_target_name
    shim_dict['type'] = 'static_library'
    shim_dict['sources'] = [ shim_cc_path ]
    shim_dict['dependencies'] = [ full_copy_target_name ]

    # Set up the shim to output its PDB to the same location as the final linker
    # target.
    for config_name, config in shim_dict.get('configurations').items():
      pdb_path = _GetPdbPath(target_dict, config_name, gyp_vars)

      # A few keys that we don't want to propagate.
      for key in ['msvs_precompiled_header', 'msvs_precompiled_source', 'test']:
        config.pop(key, None)

      msvs = config.setdefault('msvs_settings', {})

      # Update the compiler directives in the shim target.
      compiler = msvs.setdefault('VCCLCompilerTool', {})
      compiler['DebugInformationFormat'] = '3'
      compiler['ProgramDataBaseFileName'] = pdb_path

      # Set the explicit PDB path in the appropriate configuration of the
      # original target.
      config = target_dict['configurations'][config_name]
      msvs = config.setdefault('msvs_settings', {})
      linker = msvs.setdefault('VCLinkerTool', {})
      linker['GenerateDebugInformation'] = 'true'
      linker['ProgramDatabaseFile'] = pdb_path

    # Add the new targets. They must go to the beginning of the list so that
    # the dependency generation works as expected in ninja.
    target_list.insert(0, full_copy_target_name)
    target_list.insert(0, full_shim_target_name)
    target_dicts[full_copy_target_name] = copy_dict
    target_dicts[full_shim_target_name] = shim_dict

    # Update the original target to depend on the shim target.
    target_dict.setdefault('dependencies', []).append(full_shim_target_name)

  return target_list, target_dicts


def TryQueryRegistryValue(key, value=None, root=winreg.HKEY_LOCAL_MACHINE):
  try:
    with winreg.OpenKey(root, key) as kh:
      value, value_type = winreg.QueryValueEx(kh, value)
      return value
  except WindowsError:
    return None
  except NotImplementedError:
    return None


def FindVisualStudioInstallation():
  """
  Returns appropriate values for .build_tool and .uses_msbuild fields
  of TestGypBase for Visual Studio.

  We use the value specified by GYP_MSVS_VERSION.  If not specified, we
  search for likely deployment paths.
  """
  msvs_version = 'auto'
  for flag in (f for f in sys.argv if f.startswith('msvs_version=')):
    msvs_version = flag.split('=')[-1]
  msvs_version = os.environ.get('GYP_MSVS_VERSION', msvs_version)

  override_build_tool = os.environ.get('GYP_BUILD_TOOL')
  if override_build_tool:
    return override_build_tool, True, override_build_tool, msvs_version

  if msvs_version == 'auto' or msvs_version >= '2017':
    msbuild_exes = []
    top_vs_info = VSSetup_PowerShell()
    if top_vs_info:
      inst_path = top_vs_info['InstallationPath']
      args2 = ['cmd.exe', '/d', '/c',
               'cd', '/d', inst_path,
               '&', 'dir', '/b', '/s', 'msbuild.exe']
      msbuild_exes = subprocess.check_output(args2).strip().splitlines()
    if len(msbuild_exes):
      msbuild_path = str(msbuild_exes[0].decode('utf-8'))
      os.environ['GYP_MSVS_VERSION'] = top_vs_info['CatalogVersion']
      os.environ['GYP_BUILD_TOOL'] = msbuild_path
      return msbuild_path, True, msbuild_path, msvs_version

  possible_roots = ['%s:\\Program Files%s' % (chr(drive), suffix)
                    for drive in range(ord('C'), ord('Z') + 1)
                    for suffix in ['', ' (x86)']]
  possible_paths = {
    '2015': r'Microsoft Visual Studio 14.0\Common7\IDE\devenv.com',
    '2013': r'Microsoft Visual Studio 12.0\Common7\IDE\devenv.com',
    '2012': r'Microsoft Visual Studio 11.0\Common7\IDE\devenv.com',
    '2010': r'Microsoft Visual Studio 10.0\Common7\IDE\devenv.com',
    '2008': r'Microsoft Visual Studio 9.0\Common7\IDE\devenv.com',
    '2005': r'Microsoft Visual Studio 8\Common7\IDE\devenv.com'
  }

  # Check that the path to the specified GYP_MSVS_VERSION exists.
  if msvs_version in possible_paths:
    path = possible_paths[msvs_version]
    for r in possible_roots:
      build_tool = os.path.join(r, path)
      if os.path.exists(build_tool):
        uses_msbuild = msvs_version >= '2010'
        msbuild_path = FindMSBuildInstallation(msvs_version)
        return build_tool, uses_msbuild, msbuild_path, msvs_version
    else:
      print('Warning: Environment variable GYP_MSVS_VERSION specifies "%s" '
            'but corresponding "%s" was not found.' % (msvs_version, path))
  print('Error: could not find MSVS version %s' % msvs_version)
  sys.exit(1)


def FindMSBuildInstallation(msvs_version = 'auto'):
  """Returns path to MSBuild for msvs_version or latest available.

  Looks in the registry to find install location of MSBuild.
  MSBuild before v4.0 will not build c++ projects, so only use newer versions.
  """

  msvs_to_msbuild = {
    '2015': '14.0',
    '2013': '12.0',
    '2012': '4.0',  # Really v4.0.30319 which comes with .NET 4.5.
    '2010': '4.0'
  }

  msbuild_base_key = r'SOFTWARE\Microsoft\MSBuild\ToolsVersions'
  if not TryQueryRegistryValue(msbuild_base_key):
    print('Error: could not find MSBuild base registry entry')
    return None

  msbuild_key = ''
  found_msbuild_ver = ''
  if msvs_version in msvs_to_msbuild:
    msbuild_test_version = msvs_to_msbuild[msvs_version]
    msbuild_key = msbuild_base_key + '\\' + msbuild_test_version
    if TryQueryRegistryValue(msbuild_key):
      found_msbuild_ver = msbuild_test_version
    else:
      print('Warning: Environment variable GYP_MSVS_VERSION specifies "%s" but corresponding MSBuild "%s" was not found.' % (msvs_version, found_msbuild_ver))
  if not found_msbuild_ver:
    for msvs_version in sorted(msvs_to_msbuild.keys(), reverse=True):
      msbuild_test_version = msvs_to_msbuild[msvs_version]
      msbuild_key = msbuild_base_key + '\\' + msbuild_test_version
      if TryQueryRegistryValue(msbuild_key):
        found_msbuild_ver = msbuild_test_version
        break
  if not found_msbuild_ver:
    print('Error: could not find am MSBuild registry entry')
    return None

  msbuild_path = TryQueryRegistryValue(msbuild_key, 'MSBuildToolsPath')
  if not msbuild_path:
    print('Error: could not get MSBuildToolsPath registry entry value for MSBuild version %s' % found_msbuild_ver)
    return None

  return os.path.join(msbuild_path, 'MSBuild.exe')


@memoize
def VSWhere(component=None):
  args1 = [
    r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe',
    '-all', '-latest', '-sort', '-format', 'json',
  ]
  if not component:
    args1.append('-legacy')

  try:
    vswhere_json = subprocess.check_output(args1)
    vswhere_infos = json.loads(vswhere_json)
    if len(vswhere_infos) == 0:
      raise IOError("vswhere did not find any MSVS instances.")
    return vswhere_infos[0]
  except subprocess.CalledProcessError as e:
    traceback.print_exc(file=sys.stderr)
    print(e, file=sys.stderr)
    return None


new_vs_map = {
  16: '2019',
  15: '2017',
  14: '2015',
}


@memoize
def VSSetup_PowerShell():
  powershell = os.path.join(os.environ['SystemRoot'], 'System32', 'WindowsPowerShell',  'v1.0', 'powershell.exe')
  query_script_path = os.path.join(os.path.dirname(__file__), '..', '..', 'tools', 'vssetup.powershell', 'VSQuery.ps1')
  args = [
    powershell, '-ExecutionPolicy', 'Unrestricted', '-NoProfile',
    query_script_path
  ]
  try:
    vs_query_json = subprocess.check_output(args)
  except subprocess.CalledProcessError as e:
    print(e, file=sys.stderr)
    print(e.stderr, file=sys.stderr)
    raise e
  try:
    vs_query_infos = json.loads(vs_query_json)
  except json.decoder.JSONDecodeError as e:
    print(e, file=sys.stderr)
    print(vs_query_json, file=sys.stderr)
    raise e
  assert vs_query_infos
  if isinstance(vs_query_infos, dict):
    vs_query_infos = [vs_query_infos]
  if len(vs_query_infos) == 0:
    raise IOError("vssetup.powershell did not find any MSVS instances.")
  for ver in vs_query_infos:
    ver['CatalogVersion'] = new_vs_map[ver['InstallationVersion']['Major']]
  return vs_query_infos[0]

