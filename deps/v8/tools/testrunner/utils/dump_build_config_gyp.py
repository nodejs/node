# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The same as dump_build_config.py but for gyp legacy.

Expected to be called like:
dump_build_config.py path/to/file.json [key1=value1 ...]

Raw gyp values are supported - they will be tranformed into valid json.
"""
# TODO(machenbach): Remove this when gyp is deprecated.

import json
import os
import sys

assert len(sys.argv) > 2


GYP_GN_CONVERSION = {
  'is_component_build': {
    'shared_library': 'true',
    'static_library': 'false',
  },
  'is_debug': {
    'Debug': 'true',
    'Release': 'false',
  },
}

DEFAULT_CONVERSION ={
  '0': 'false',
  '1': 'true',
  'ia32': 'x86',
}

def gyp_to_gn(key, value):
  value = GYP_GN_CONVERSION.get(key, DEFAULT_CONVERSION).get(value, value)
  value = value if value in ['true', 'false'] else '"{0}"'.format(value)
  return value

def as_json(kv):
  assert '=' in kv
  k, v = kv.split('=', 1)
  v2 = gyp_to_gn(k, v)
  try:
    return k, json.loads(v2)
  except ValueError as e:
    print(k, v, v2)
    raise e

with open(sys.argv[1], 'w') as f:
  json.dump(dict(map(as_json, sys.argv[2:])), f)
