#!/usr/bin/env python

import os
import platform
import sys

try:
  import multiprocessing.synchronize
  gyp_parallel_support = True
except ImportError:
  gyp_parallel_support = False


CC = os.environ.get('CC', 'cc')
script_dir = os.path.dirname(__file__)
uv_root = os.path.normpath(script_dir)
output_dir = os.path.join(os.path.abspath(uv_root), 'out')

sys.path.insert(0, os.path.join(uv_root, 'build', 'gyp', 'pylib'))
try:
  import gyp
except ImportError:
  print('You need to install gyp in build/gyp first. See the README.')
  sys.exit(42)


def host_arch():
  machine = platform.machine()
  if machine == 'i386': return 'ia32'
  if machine == 'AMD64': return 'x64'
  if machine == 'x86_64': return 'x64'
  if machine.startswith('arm'): return 'arm'
  if machine.startswith('mips'): return 'mips'
  return machine  # Return as-is and hope for the best.


def run_gyp(args):
  rc = gyp.main(args)
  if rc != 0:
    print('Error running GYP')
    sys.exit(rc)


if __name__ == '__main__':
  args = sys.argv[1:]
  args.extend('-I common.gypi test/test.gyp'.split(' '))
  args.append('--depth=' + uv_root)

  # There's a bug with windows which doesn't allow this feature.
  if sys.platform != 'win32':
    if '-f' not in args:
      args.extend('-f make'.split())
    if 'eclipse' not in args and 'ninja' not in args:
      args.extend(['-Goutput_dir=' + output_dir])
      args.extend(['--generator-output', output_dir])

  if not any(a.startswith('-Dhost_arch=') for a in args):
    args.append('-Dhost_arch=%s' % host_arch())

  if not any(a.startswith('-Dtarget_arch=') for a in args):
    args.append('-Dtarget_arch=%s' % host_arch())

  if not any(a.startswith('-Duv_library=') for a in args):
    args.append('-Duv_library=static_library')

  # Some platforms (OpenBSD for example) don't have multiprocessing.synchronize
  # so gyp must be run with --no-parallel
  if not gyp_parallel_support:
    args.append('--no-parallel')

  gyp_args = list(args)
  print(gyp_args)
  run_gyp(gyp_args)
