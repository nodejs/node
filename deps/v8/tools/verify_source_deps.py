#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Script to print potentially missing source dependencies based on the actual
.h and .cc files in the source tree and which files are included in the gyp
and gn files. The latter inclusion is overapproximated.

TODO(machenbach): Gyp files in src will point to source files in src without a
src/ prefix. For simplicity, all paths relative to src are stripped. But this
tool won't be accurate for other sources in other directories (e.g. cctest).
"""

import itertools
import re
import os


V8_BASE = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
V8_SRC_BASE = os.path.join(V8_BASE, 'src')
V8_INCLUDE_BASE = os.path.join(V8_BASE, 'include')

GYP_FILES = [
  os.path.join(V8_BASE, 'src', 'd8.gyp'),
  os.path.join(V8_BASE, 'src', 'third_party', 'vtune', 'v8vtune.gyp'),
  os.path.join(V8_BASE, 'test', 'cctest', 'cctest.gyp'),
  os.path.join(V8_BASE, 'test', 'unittests', 'unittests.gyp'),
  os.path.join(V8_BASE, 'tools', 'gyp', 'v8.gyp'),
  os.path.join(V8_BASE, 'tools', 'parser-shell.gyp'),
]


def path_no_prefix(path):
  if path.startswith('../'):
    return path_no_prefix(path[3:])
  elif path.startswith('src/'):
    return path_no_prefix(path[4:])
  else:
    return path


def isources(directory):
  for root, dirs, files in os.walk(directory):
    for f in files:
      if not (f.endswith('.h') or f.endswith('.cc')):
        continue
      yield path_no_prefix(os.path.relpath(os.path.join(root, f), V8_BASE))


def iflatten(obj):
  if isinstance(obj, dict):
    for value in obj.values():
      for i in iflatten(value):
        yield i
  elif isinstance(obj, list):
    for value in obj:
      for i in iflatten(value):
        yield i
  elif isinstance(obj, basestring):
    yield path_no_prefix(obj)


def iflatten_gyp_file(gyp_file):
  """Overaproximates all values in the gyp file.

  Iterates over all string values recursively. Removes '../' path prefixes.
  """
  with open(gyp_file) as f:
    return iflatten(eval(f.read()))


def iflatten_gn_file(gn_file):
  """Overaproximates all values in the gn file.

  Iterates over all double quoted strings.
  """
  with open(gn_file) as f:
    for line in f.read().splitlines():
      match = re.match(r'.*"([^"]*)".*', line)
      if match:
        yield path_no_prefix(match.group(1))


def icheck_values(values, *source_dirs):
  for source_file in itertools.chain(
      *[isources(source_dir) for source_dir in source_dirs]
    ):
    if source_file not in values:
      yield source_file


gyp_values = set(itertools.chain(
  *[iflatten_gyp_file(gyp_file) for gyp_file in GYP_FILES]
  ))

print "----------- Files not in gyp: ------------"
for i in sorted(icheck_values(gyp_values, V8_SRC_BASE, V8_INCLUDE_BASE)):
  print i

gn_values = set(iflatten_gn_file(os.path.join(V8_BASE, 'BUILD.gn')))

print "\n----------- Files not in gn: -------------"
for i in sorted(icheck_values(gn_values, V8_SRC_BASE, V8_INCLUDE_BASE)):
  print i
