#!/usr/bin/env python
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Legacy test-runner wrapper supporting a product of multiple architectures and
modes.
"""

import argparse
import itertools
from os.path import abspath, dirname, join
import subprocess
import sys

BASE_DIR = dirname(dirname(abspath(__file__)))
RUN_TESTS = join(BASE_DIR, 'tools', 'run-tests.py')

def main():
  parser = argparse.ArgumentParser(description='Legacy test-runner wrapper')
  parser.add_argument(
      '--arch', help='Comma-separated architectures to run tests on')
  parser.add_argument(
      '--mode', help='Comma-separated modes to run tests on')
  parser.add_argument(
      '--arch-and-mode',
      help='Architecture and mode in the format \'arch.mode\'',
  )

  args, remaining_args = parser.parse_known_args(sys.argv)
  if (args.arch or args.mode) and args.arch_and_mode:
    parser.error('The flags --arch-and-mode and --arch/--mode are exclusive.')
  arch = (args.arch or 'ia32,x64,arm').split(',')
  mode = (args.mode or 'release,debug').split(',')
  if args.arch_and_mode:
    arch_and_mode = map(
        lambda am: am.split('.'),
        args.arch_and_mode.split(','))
    arch = map(lambda am: am[0], arch_and_mode)
    mode = map(lambda am: am[1], arch_and_mode)

  ret_code = 0
  for a, m in itertools.product(arch, mode):
    ret_code |= subprocess.check_call(
        [RUN_TESTS] + remaining_args[1:] + ['--arch', a, '--mode', m])
  return ret_code

if __name__ == '__main__':
  sys.exit(main())
