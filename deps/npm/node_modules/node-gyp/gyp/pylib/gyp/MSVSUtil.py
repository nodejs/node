# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions shared amongst the Windows generators."""

import copy
import os


_TARGET_TYPE_EXT = {
  'executable': '.exe',
  'shared_library': '.dll'
}


def _GetLargePdbShimCcPath():
  """Returns the path of the large_pdb_shim.cc file."""
  this_dir = os.path.abspath(os.path.dirname(__file__))
  src_dir = os.path.abspath(os.path.join(this_dir, '..', '..'))
  win_data_dir = os.path.join(src_dir, 'data', 'win')
  large_pdb_shim_cc = os.path.join(win_data_dir, 'large-pdb-shim.cc')
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
  for t in new_target_dicts:
    dependencies = copy.copy(new_target_dicts[t].get('dependencies', []))
    new_dependencies = []
    for d in dependencies:
      if d in targets_to_shard:
        for i in range(targets_to_shard[d]):
          new_dependencies.append(_ShardName(d, i))
      else:
        new_dependencies.append(d)
    new_target_dicts[t]['dependencies'] = new_dependencies

  return (new_target_list, new_target_dicts)


def InsertLargePdbShims(target_list, target_dicts, vars):
  """Insert a shim target that forces the linker to use 4KB pagesize PDBs.

  This is a workaround for targets with PDBs greater than 1GB in size, the
  limit for the 1KB pagesize PDBs created by the linker by default.

  Arguments:
    target_list: List of target pairs: 'base/base.gyp:base'.
    target_dicts: Dict of target properties keyed on target pair.
    vars: A dictionary of common GYP variables with generator-specific values.
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
    copy_suffix = '_large_pdb_copy'
    copy_target_name = target_name + '_' + copy_suffix
    full_copy_target_name = _SuffixName(t, copy_suffix)
    shim_cc_basename = os.path.basename(large_pdb_shim_cc)
    shim_cc_dir = vars['SHARED_INTERMEDIATE_DIR'] + '/' + copy_target_name
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
    shim_suffix = '_large_pdb_shim'
    shim_target_name = target_name + '_' + shim_suffix
    full_shim_target_name = _SuffixName(t, shim_suffix)
    shim_dict = copy.deepcopy(base_dict)
    shim_dict['target_name'] = shim_target_name
    shim_dict['type'] = 'static_library'
    shim_dict['sources'] = [ shim_cc_path ]
    shim_dict['dependencies'] = [ full_copy_target_name ]

    # Set up the shim to output its PDB to the same location as the final linker
    # target.
    for config in shim_dict.get('configurations').itervalues():
      msvs = config.setdefault('msvs_settings')

      linker = msvs.pop('VCLinkerTool')  # We want to clear this dict.
      pdb_path = linker.get('ProgramDatabaseFile')

      compiler = msvs.setdefault('VCCLCompilerTool', {})
      compiler.setdefault('DebugInformationFormat', '3')
      compiler.setdefault('ProgramDataBaseFileName', pdb_path)

    # Add the new targets.
    target_list.append(full_copy_target_name)
    target_list.append(full_shim_target_name)
    target_dicts[full_copy_target_name] = copy_dict
    target_dicts[full_shim_target_name] = shim_dict

    # Update the original target to depend on the shim target.
    target_dict.setdefault('dependencies', []).append(full_shim_target_name)

  return (target_list, target_dicts)