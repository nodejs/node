# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import os.path


def GetNinjaOutputDirectory(v8_root, configuration=None):
  """Returns <v8_root>/<output_dir>/(Release|Debug).

  The configuration chosen is the one most recently generated/built, but can be
  overriden via the <configuration> parameter. Detects a custom output_dir
  specified by GYP_GENERATOR_FLAGS."""

  output_dir = 'out'
  generator_flags = os.getenv('GYP_GENERATOR_FLAGS', '').split(' ')
  for flag in generator_flags:
    name_value = flag.split('=', 1)
    if len(name_value) == 2 and name_value[0] == 'output_dir':
      output_dir = name_value[1]

  root = os.path.join(v8_root, output_dir)
  if configuration:
    return os.path.join(root, configuration)

  debug_path = os.path.join(root, 'Debug')
  release_path = os.path.join(root, 'Release')

  def is_release_newer(test_path):
    try:
      debug_mtime = os.path.getmtime(os.path.join(debug_path, test_path))
    except os.error:
      debug_mtime = 0
    try:
      rel_mtime = os.path.getmtime(os.path.join(release_path, test_path))
    except os.error:
      rel_mtime = 0
    return rel_mtime >= debug_mtime

  if is_release_newer('.ninja_log') or is_release_newer('.ninja_deps'):
    return release_path
  return debug_path
