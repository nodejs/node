#!/usr/bin/env python3
# Copyright (c) 2013-2019 GitHub Inc.
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script reads the configurations of GN and outputs a config.gypi file that
# will be used to populate process.config.variables.

import re
import os
import subprocess
import sys

root_dir = os.path.dirname(os.path.dirname(__file__))
sys.path.append(os.path.join(root_dir, 'node', 'tools'))
import getmoduleversion
import getnapibuildversion

GN_RE = re.compile(r'(\w+)\s+=\s+(.*?)$', re.MULTILINE)

def bool_string_to_number(v):
  return 1 if v == 'true' else 0

def translate_config(config):
  return {
    'target_defaults': {
      'default_configuration':
          'Debug' if config['is_debug'] == 'true' else 'Release',
    },
    'variables': {
      'asan': bool_string_to_number(config['is_asan']),
      'llvm_version': 13,
      'napi_build_version': config['napi_build_version'],
      'node_builtin_shareable_builtins':
          eval(config['node_builtin_shareable_builtins']),
      'node_module_version': int(config['node_module_version']),
      'node_shared': bool_string_to_number(config['is_component_build']),
      'node_use_openssl': config['node_use_openssl'],
      'node_use_node_code_cache': config['node_use_node_code_cache'],
      'node_use_node_snapshot': config['node_use_node_snapshot'],
      'v8_enable_31bit_smis_on_64bit_arch':
          bool_string_to_number(config['v8_enable_31bit_smis_on_64bit_arch']),
      'v8_enable_pointer_compression':
          bool_string_to_number(config['v8_enable_pointer_compression']),
      'v8_enable_i18n_support':
          bool_string_to_number(config['v8_enable_i18n_support']),
      'v8_enable_inspector':  # this is actually a node misnomer
          bool_string_to_number(config['node_enable_inspector']),
      'shlib_suffix': 'dylib' if sys.platform == 'darwin' else 'so',
    }
  }

def main(gn_out_dir, output_file, depfile):
  # Get GN config and parse into a dictionary.
  if sys.platform == 'win32':
    gn = 'gn.exe'
  else:
    gn = 'gn'
  gnconfig = subprocess.check_output(
                 [gn, 'args', '--list', '--short', '-C', gn_out_dir])
  config = dict(re.findall(GN_RE, gnconfig.decode('utf-8')))
  config['node_module_version'] = getmoduleversion.get_version()
  config['napi_build_version'] = getnapibuildversion.get_napi_version()

  # Write output.
  with open(output_file, 'w') as f:
    f.write(repr(translate_config(config)))

  # Write depfile. Force regenerating config.gypi when GN configs change.
  with open(depfile, 'w') as f:
    f.write('%s: %s '%(output_file, 'build.ninja'))

if __name__ == '__main__':
  main(sys.argv[1], sys.argv[2], sys.argv[3])
