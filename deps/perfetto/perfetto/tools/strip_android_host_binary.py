#!/usr/bin/env python
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function

import argparse
import os
import subprocess
import sys

THIS_DIR = os.path.realpath(os.path.dirname(__file__))


def android_build_top():
  return os.path.realpath(os.path.join(THIS_DIR, '../../..'))


def clang_build():
  gofile = os.path.join(android_build_top(), 'build', 'soong', 'cc', 'config',
                        'global.go')
  try:
    with open(gofile) as f:
      lines = f.readlines()
      versionLine = [l for l in lines if 'ClangDefaultVersion' in l][0]
      start, end = versionLine.index('"'), versionLine.rindex('"')
      return versionLine[start + 1:end]
  except Exception as err:
    raise RuntimeError("Extracting Clang version failed with {0}".format(err))


def llvm_strip():
  return os.path.join(android_build_top(), 'prebuilts', 'clang', 'host',
                      'linux-x86', clang_build(), 'bin', 'llvm-strip')


def main():
  parser = argparse.ArgumentParser(
      description='Strips a binary in the Android tree.')
  parser.add_argument(
      '-o', dest='output', default=None, help='Output file', required=True)
  parser.add_argument('binary', type=str, help='location of binary')
  args = parser.parse_args()
  return subprocess.call([llvm_strip(), args.binary, '-o', args.output])


if __name__ == '__main__':
  sys.exit(main())
