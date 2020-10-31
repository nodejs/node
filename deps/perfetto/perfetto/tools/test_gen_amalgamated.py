#!/usr/bin/env python
# Copyright (C) 2019 The Android Open Source Project
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

import os
import shutil
import subprocess

from compat import quote
from platform import system

GN_ARGS = ' '.join(
    quote(s) for s in (
        'is_debug=false',
        'is_perfetto_build_generator=true',
        'is_perfetto_embedder=true',
        'use_custom_libcxx=false',
        'enable_perfetto_ipc=true',
    ))

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
OUT_DIR = os.path.join('out', 'amalgamated')
GEN_AMALGAMATED = os.path.join('tools', 'gen_amalgamated')


def call(cmd, *args):
  command = [cmd] + list(args)
  print('Running:', ' '.join(quote(c) for c in command))
  try:
    return subprocess.check_output(command, cwd=ROOT_DIR).decode()
  except subprocess.CalledProcessError as e:
    assert False, 'Command: %s failed: %s'.format(' '.join(command))


def check_amalgamated_output():
  call(GEN_AMALGAMATED, '--quiet')


def check_amalgamated_build():
  args = [
      '-std=c++11', '-Werror', '-Wall', '-Wextra',
      '-DPERFETTO_AMALGAMATED_SDK_TEST', '-I' + OUT_DIR,
      OUT_DIR + '/perfetto.cc', 'test/client_api_example.cc', '-o',
      OUT_DIR + '/test'
  ]
  if system().lower() == 'linux':
    args += ['-lpthread', '-lrt']
  call('clang++', *args)


def check_amalgamated_dependencies():
  os_deps = {}
  for os_name in ['android', 'linux', 'mac']:
    gn_args = (' target_os="%s"' % os_name) + GN_ARGS
    os_deps[os_name] = call(GEN_AMALGAMATED, '--gn_args', gn_args, '--out',
                            OUT_DIR, '--dump-deps', '--quiet').split('\n')
  for os_name, deps in os_deps.items():
    for dep in deps:
      for other_os, other_deps in os_deps.items():
        if not dep in other_deps:
          raise AssertionError('Discrepancy in amalgamated build dependencies: '
                               '%s is missing on %s.' % (dep, other_os))


def main():
  check_amalgamated_dependencies()
  check_amalgamated_output()
  check_amalgamated_build()
  shutil.rmtree(OUT_DIR)


if __name__ == '__main__':
  exit(main())
