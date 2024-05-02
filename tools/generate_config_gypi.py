#!/usr/bin/env python3
# Copyright (c) 2013-2019 GitHub Inc.
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script reads the configurations of GN and outputs a config.gypi file that
# will be used to populate process.config.variables.

import argparse
import re
import os
import subprocess
import sys

sys.path.append(os.path.dirname(__file__))
import getnapibuildversion

# The defines bellow must include all things from the external_v8_defines list
# in v8/BUILD.gn.
# TODO(zcbenz): Import from v8_features.json once this change gets into Node:
# https://chromium-review.googlesource.com/c/v8/v8/+/5040612
V8_FEATURE_DEFINES = {
  'v8_enable_v8_checks': 'V8_ENABLE_CHECKS',
  'v8_enable_pointer_compression': 'V8_COMPRESS_POINTERS',
  'v8_enable_pointer_compression_shared_cage': 'V8_COMPRESS_POINTERS_IN_SHARED_CAGE',
  'v8_enable_31bit_smis_on_64bit_arch': 'V8_31BIT_SMIS_ON_64BIT_ARCH',
  'v8_enable_zone_compression': 'V8_COMPRESS_ZONES',
  'v8_enable_sandbox': 'V8_ENABLE_SANDBOX',
  'v8_deprecation_warnings': 'V8_DEPRECATION_WARNINGS',
  'v8_imminent_deprecation_warnings': 'V8_IMMINENT_DEPRECATION_WARNINGS',
  'v8_use_perfetto': 'V8_USE_PERFETTO',
  'v8_enable_map_packing': 'V8_MAP_PACKING',
  'tsan': 'V8_IS_TSAN',
  'v8_enable_conservative_stack_scanning': 'V8_ENABLE_CONSERVATIVE_STACK_SCANNING',
  'v8_enable_direct_local': 'V8_ENABLE_DIRECT_LOCAL',
}

# Regex used for parsing results of "gn args".
GN_RE = re.compile(r'(\w+)\s+=\s+(.*?)$', re.MULTILINE)

if sys.platform == 'win32':
  GN = 'gn.exe'
else:
  GN = 'gn'

def bool_to_number(v):
  return 1 if v else 0

def bool_string_to_number(v):
  return bool_to_number(v == 'true')

def get_gn_config(out_dir):
  # Read args from GN configurations.
  gn_args = subprocess.check_output(
      [GN, 'args', '--list', '--short', '-C', out_dir])
  config = dict(re.findall(GN_RE, gn_args.decode()))
  # Get napi_build_version from Node, which is not part of GN args.
  config['napi_build_version'] = getnapibuildversion.get_napi_version()
  return config

def get_v8_config(out_dir, node_gn_path):
  # For args that have default values in V8's GN configurations, we can not rely
  # on the values printed by "gn args", because most of them would be empty
  # strings, and the actual value would depend on the logics in v8/BUILD.gn.
  # So we print out the defines and deduce the feature from them instead.
  node_defines = subprocess.check_output(
      [GN, 'desc', '-C', out_dir, node_gn_path + ":libnode", 'defines']).decode().split('\n')
  v8_config = {}
  for feature, define in V8_FEATURE_DEFINES.items():
    v8_config[feature] = bool_to_number(define in node_defines)
  return v8_config

def translate_config(out_dir, config, v8_config):
  config_gypi = {
    'target_defaults': {
      'default_configuration':
          'Debug' if config['is_debug'] == 'true' else 'Release',
    },
    'variables': {
      'asan': bool_string_to_number(config['is_asan']),
      'enable_lto': config['use_thin_lto'],
      'is_debug': bool_string_to_number(config['is_debug']),
      'llvm_version': 13,
      'napi_build_version': config['napi_build_version'],
      'node_builtin_shareable_builtins':
          eval(config['node_builtin_shareable_builtins']),
      'node_module_version': int(config['node_module_version']),
      'node_use_openssl': config['node_use_openssl'],
      'node_use_node_code_cache': config['node_use_node_code_cache'],
      'node_use_node_snapshot': config['node_use_node_snapshot'],
      'v8_enable_i18n_support':
          bool_string_to_number(config['v8_enable_i18n_support']),
      'v8_enable_inspector':  # this is actually a node misnomer
          bool_string_to_number(config['node_enable_inspector']),
      'shlib_suffix': 'dylib' if sys.platform == 'darwin' else 'so',
      'tsan': bool_string_to_number(config['is_tsan']),
      # TODO(zcbenz): Shared components are not supported in GN config yet.
      'node_shared': 'false',
      'node_shared_brotli': 'false',
      'node_shared_cares': 'false',
      'node_shared_http_parser': 'false',
      'node_shared_libuv': 'false',
      'node_shared_nghttp2': 'false',
      'node_shared_nghttp3': 'false',
      'node_shared_ngtcp2': 'false',
      'node_shared_openssl': 'false',
      'node_shared_zlib': 'false',
    }
  }
  config_gypi['variables'].update(v8_config)
  return config_gypi

def main():
  parser = argparse.ArgumentParser(
      description='Generate config.gypi file from GN configurations')
  parser.add_argument('target', help='path to generated config.gypi file')
  parser.add_argument('--out-dir', help='path to the output directory',
                      default='out/Release')
  parser.add_argument('--node-gn-path', help='path of the node target in GN',
                      default='//node')
  parser.add_argument('--dep-file', help='path to an optional dep file',
                      default=None)
  args, unknown_args = parser.parse_known_args()

  config = get_gn_config(args.out_dir)
  v8_config = get_v8_config(args.out_dir, args.node_gn_path)

  # Write output.
  with open(args.target, 'w') as f:
    f.write(repr(translate_config(args.out_dir, config, v8_config)))

  # Write depfile. Force regenerating config.gypi when GN configs change.
  if args.dep_file:
    with open(args.dep_file, 'w') as f:
      f.write('%s: %s '%(args.target, 'build.ninja'))

if __name__ == '__main__':
  main()
