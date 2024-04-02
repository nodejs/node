# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes a dictionary to a json file with the passed key-value pairs.

Expected to be called like:
dump_build_config.py path/to/file.json [key1=value1 ...]

The values are expected to be valid json. E.g. true is a boolean and "true" is
the string "true".
"""

import json
import os
import sys

assert len(sys.argv) > 2

def as_json(kv):
  assert '=' in kv
  k, v = kv.split('=', 1)
  return k, json.loads(v)

with open(sys.argv[1], 'w') as f:
  json.dump(dict(map(as_json, sys.argv[2:])), f)
