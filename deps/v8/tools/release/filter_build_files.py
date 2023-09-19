#!/usr/bin/env python3
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Enumerates relevant build files for each platform.

This can be used to filter the build directory before making an official
archive. The archive should only contain files required for running or
static linking, e.g. executables, startup files, libraries.

The script is limited to release builds and assumes GN.
"""

import argparse
import glob
import itertools
import json
import os
import re
import sys

EXECUTABLE_FILES = [
  'd8',
]

# Additional executable files added only to ref archive type.
REFBUILD_EXECUTABLE_FILES = [
  'cctest',
]

SUPPLEMENTARY_FILES = [
  'icudtl.dat',
  'snapshot_blob.bin',
  'v8_build_config.json',
]

LIBRARY_FILES = {
  'android': ['*.a', '*.so'],
  'linux': ['*.a', '*.so'],
  'mac': ['*.a', '*.so', '*.dylib'],
  'win': ['*.lib', '*.dll'],
}


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)

  parser.add_argument('-d', '--dir', required=True,
                      help='Path to the build directory.')
  parser.add_argument('-p', '--platform', required=True,
                      help='Target platform name: win|mac|linux.')
  parser.add_argument('-o', '--json-output', required=True,
                      help='Path to an output file. The files will '
                           'be stored in json list with absolute paths.')
  parser.add_argument('-t', '--type',
                      choices=['all', 'exe', 'lib', 'ref'], default='all',
                      help='Specifies the archive type.')
  args = parser.parse_args()

  if not os.path.isdir(args.dir):
    parser.error('%s is not an existing directory.' % args.dir)

  args.dir = os.path.abspath(args.dir)

  # Skip libraries for exe and ref archive types.
  if args.type in ('exe', 'ref'):
    library_files = []
  else:
    library_files = LIBRARY_FILES[args.platform]

  # Skip executables for lib archive type.
  if args.type == 'lib':
    executable_files = []
  else:
    executable_files = EXECUTABLE_FILES

  if args.type == 'ref':
    executable_files.extend(REFBUILD_EXECUTABLE_FILES)

  list_of_files = []
  def add_files_from_globs(globs):
    list_of_files.extend(itertools.chain(*map(glob.iglob, globs)))

  # Add toplevel executables, supplementary files and libraries.
  extended_executable_files = [
    f + '.exe' if args.platform == 'win' else f
    for f in executable_files]
  add_files_from_globs(
      os.path.join(args.dir, f)
      for f in extended_executable_files +
               SUPPLEMENTARY_FILES +
               library_files
  )

  # Add libraries recursively from obj directory.
  for root, _, __ in os.walk(os.path.join(args.dir, 'obj'), followlinks=True):
    add_files_from_globs(os.path.join(root, g) for g in library_files)

  with open(args.json_output, 'w') as f:
    json.dump(list_of_files, f)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
