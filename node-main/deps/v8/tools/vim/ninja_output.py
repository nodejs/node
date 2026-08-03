# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import sys
import os
import re

try:
  from exceptions import RuntimeError
except ImportError:
  pass


def GetNinjaOutputDirectory(v8_root, configuration=None):
  """Returns <v8_root>/<output_dir>/(Release|Debug|<other>).

  The configuration chosen is the one most recently generated/built, but can be
  overriden via the <configuration> parameter. Detects a custom output_dir
  specified by GYP_GENERATOR_FLAGS."""

  output_dirs = []

  generator_flags = os.getenv('GYP_GENERATOR_FLAGS', '').split(' ')
  for flag in generator_flags:
    name_value = flag.split('=', 1)
    if (len(name_value) == 2 and name_value[0] == 'output_dir' and
        os.path.isdir(os.path.join(v8_root, name_value[1]))):
      output_dirs = [name_value[1]]

  if configuration:
    output_dir = 'out' if len(output_dirs) == 0 else output_dirs[-1]
    return os.path.join(os.path.join(v8_root, output_dir), configuration)

  if not output_dirs:
    for f in os.listdir(v8_root):
      if re.match(r'out(\b|_)', f):
        if os.path.isdir(os.path.join(v8_root, f)):
          output_dirs.append(f)

  def generate_paths():
    for out_dir in output_dirs:
      out_path = os.path.join(v8_root, out_dir)
      for config in os.listdir(out_path):
        path = os.path.join(out_path, config)
        if os.path.exists(os.path.join(path, 'build.ninja')):
          yield path

  def approx_directory_mtime(path):
    # This is a heuristic; don't recurse into subdirectories.
    paths = [path] + [os.path.join(path, f) for f in os.listdir(path)]
    return max(filter(None, [safe_mtime(p) for p in paths]))

  def safe_mtime(path):
    try:
      return os.path.getmtime(path)
    except OSError:
      return None

  try:
    return max(generate_paths(), key=approx_directory_mtime)
  except ValueError:
    raise RuntimeError('Unable to find a valid ninja output directory.')


if __name__ == '__main__':
  if len(sys.argv) != 2:
    raise RuntimeError('Expected a single path argument.')
  print(GetNinjaOutputDirectory(sys.argv[1]))
