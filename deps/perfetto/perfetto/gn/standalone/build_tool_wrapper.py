#!/usr/bin/env python
# Copyright (C) 2017 The Android Open Source Project
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
""" Wrapper to invoke compiled build tools from the build system.

This is just a workaround for GN assuming that all external scripts are
python sources. It is used to invoke tools like the protoc compiler.
"""

from __future__ import print_function

import argparse
import os
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--chdir', default=None)
  parser.add_argument('--stamp', default=None)
  parser.add_argument('--path', default=None)
  parser.add_argument('--noop', default=False, action='store_true')
  parser.add_argument('--suppress_stdout', default=False, action='store_true')
  parser.add_argument('--suppress_stderr', default=False, action='store_true')
  parser.add_argument('cmd', nargs=argparse.REMAINDER)
  args = parser.parse_args()

  if args.noop:
    return 0

  if args.chdir and not os.path.exists(args.chdir):
    print(
        'Cannot chdir to %s from %s' % (workdir, os.getcwd()), file=sys.stderr)
    return 1

  exe = os.path.abspath(args.cmd[0]) if os.sep in args.cmd[0] else args.cmd[0]
  env = os.environ.copy()
  if args.path:
    env['PATH'] = os.path.abspath(args.path) + os.pathsep + env['PATH']

  devnull = open(os.devnull, 'wb')
  stdout = devnull if args.suppress_stdout else None
  stderr = devnull if args.suppress_stderr else None

  try:
    proc = subprocess.Popen(
        [exe] + args.cmd[1:],
        cwd=args.chdir,
        env=env,
        stderr=stderr,
        stdout=stdout)
    ret = proc.wait()
    if ret == 0 and args.stamp:
      with open(args.stamp, 'w'):
        os.utime(args.stamp, None)
    return ret
  except OSError as e:
    print('Error running: "%s" (%s)' % (args.cmd[0], e.strerror))
    print('PATH=%s' % env.get('PATH'))
    return 127


if __name__ == '__main__':
  sys.exit(main())
