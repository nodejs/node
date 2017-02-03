#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Script to print potentially missing source dependencies based on the actual
.h and .cc files in the source tree and which files are included in the gyp
and gn files. The latter inclusion is overapproximated.

TODO(machenbach): If two source files with the same name exist, but only one
is referenced from a gyp/gn file, we won't necessarily detect it.
"""

import itertools
import re
import os
import subprocess
import sys


V8_BASE = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

GYP_FILES = [
  os.path.join(V8_BASE, 'src', 'd8.gyp'),
  os.path.join(V8_BASE, 'src', 'v8.gyp'),
  os.path.join(V8_BASE, 'src', 'inspector', 'inspector.gypi'),
  os.path.join(V8_BASE, 'src', 'third_party', 'vtune', 'v8vtune.gyp'),
  os.path.join(V8_BASE, 'samples', 'samples.gyp'),
  os.path.join(V8_BASE, 'test', 'cctest', 'cctest.gyp'),
  os.path.join(V8_BASE, 'test', 'fuzzer', 'fuzzer.gyp'),
  os.path.join(V8_BASE, 'test', 'unittests', 'unittests.gyp'),
  os.path.join(V8_BASE, 'test', 'inspector', 'inspector.gyp'),
  os.path.join(V8_BASE, 'testing', 'gmock.gyp'),
  os.path.join(V8_BASE, 'testing', 'gtest.gyp'),
  os.path.join(V8_BASE, 'tools', 'parser-shell.gyp'),
]

ALL_GYP_PREFIXES = [
  '..',
  'common',
  os.path.join('src', 'third_party', 'vtune'),
  'src',
  'samples',
  'testing',
  'tools',
  os.path.join('test', 'cctest'),
  os.path.join('test', 'common'),
  os.path.join('test', 'fuzzer'),
  os.path.join('test', 'unittests'),
  os.path.join('test', 'inspector'),
]

GYP_UNSUPPORTED_FEATURES = [
  'gcmole',
]

GN_FILES = [
  os.path.join(V8_BASE, 'BUILD.gn'),
  os.path.join(V8_BASE, 'build', 'secondary', 'testing', 'gmock', 'BUILD.gn'),
  os.path.join(V8_BASE, 'build', 'secondary', 'testing', 'gtest', 'BUILD.gn'),
  os.path.join(V8_BASE, 'src', 'inspector', 'BUILD.gn'),
  os.path.join(V8_BASE, 'test', 'cctest', 'BUILD.gn'),
  os.path.join(V8_BASE, 'test', 'unittests', 'BUILD.gn'),
  os.path.join(V8_BASE, 'test', 'inspector', 'BUILD.gn'),
  os.path.join(V8_BASE, 'tools', 'BUILD.gn'),
]

GN_UNSUPPORTED_FEATURES = [
  'aix',
  'cygwin',
  'freebsd',
  'gcmole',
  'openbsd',
  'ppc',
  'qnx',
  'solaris',
  'vtune',
  'x87',
]

ALL_GN_PREFIXES = [
  '..',
  os.path.join('src', 'inspector'),
  'src',
  'testing',
  os.path.join('test', 'cctest'),
  os.path.join('test', 'unittests'),
  os.path.join('test', 'inspector'),
]

def pathsplit(path):
  return re.split('[/\\\\]', path)

def path_no_prefix(path, prefixes):
  for prefix in prefixes:
    if path.startswith(prefix + os.sep):
      return path_no_prefix(path[len(prefix) + 1:], prefixes)
  return path


def isources(prefixes):
  cmd = ['git', 'ls-tree', '-r', 'HEAD', '--full-name', '--name-only']
  for f in subprocess.check_output(cmd, universal_newlines=True).split('\n'):
    if not (f.endswith('.h') or f.endswith('.cc')):
      continue
    yield path_no_prefix(os.path.join(*pathsplit(f)), prefixes)


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
    yield path_no_prefix(os.path.join(*pathsplit(obj)), ALL_GYP_PREFIXES)


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
        yield path_no_prefix(
            os.path.join(*pathsplit(match.group(1))), ALL_GN_PREFIXES)


def icheck_values(values, prefixes):
  for source_file in isources(prefixes):
    if source_file not in values:
      yield source_file


def missing_gyp_files():
  gyp_values = set(itertools.chain(
    *[iflatten_gyp_file(gyp_file) for gyp_file in GYP_FILES]
    ))
  gyp_files = sorted(icheck_values(gyp_values, ALL_GYP_PREFIXES))
  return filter(
      lambda x: not any(i in x for i in GYP_UNSUPPORTED_FEATURES), gyp_files)


def missing_gn_files():
  gn_values = set(itertools.chain(
    *[iflatten_gn_file(gn_file) for gn_file in GN_FILES]
    ))

  gn_files = sorted(icheck_values(gn_values, ALL_GN_PREFIXES))
  return filter(
      lambda x: not any(i in x for i in GN_UNSUPPORTED_FEATURES), gn_files)


def main():
  print "----------- Files not in gyp: ------------"
  for i in missing_gyp_files():
    print i

  print "\n----------- Files not in gn: -------------"
  for i in missing_gn_files():
    print i
  return 0

if '__main__' == __name__:
  sys.exit(main())
