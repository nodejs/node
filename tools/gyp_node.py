#!/usr/bin/env python
from __future__ import print_function
import os
import sys

script_dir = os.path.dirname(__file__)
node_root  = os.path.normpath(os.path.join(script_dir, os.pardir))

sys.path.insert(0, os.path.join(node_root, 'tools', 'gyp', 'pylib'))
import gyp

# Add search path for `pymod_do_main` first to avoid depending on
# load order of gyp files.
sys.path.insert(0, os.path.join(node_root, 'tools', 'v8_gypfiles'))

# Directory within which we want all generated files (including Makefiles)
# to be written.
output_dir = os.path.join(os.path.abspath(node_root), 'out')

def run_gyp(args):
  # GYP bug.
  # On msvs it will crash if it gets an absolute path.
  # On Mac/make it will crash if it doesn't get an absolute path.
  a_path = node_root if sys.platform == 'win32' else os.path.abspath(node_root)
  args.append(os.path.join(a_path, 'node.gyp'))
  common_fn = os.path.join(a_path, 'common.gypi')
  options_fn = os.path.join(a_path, 'config.gypi')

  if os.path.exists(common_fn):
    args.extend(['-I', common_fn])

  if os.path.exists(options_fn):
    args.extend(['-I', options_fn])

  args.append('--depth=' + node_root)

  # There's a bug with windows which doesn't allow this feature.
  if sys.platform != 'win32' and 'ninja' not in args:
    # Tell gyp to write the Makefiles into output_dir
    args.extend(['--generator-output', output_dir])

    # Tell make to write its output into the same dir
    args.extend(['-Goutput_dir=' + output_dir])

  args.append('-Dcomponent=static_library')
  args.append('-Dlibrary=static_library')

  rc = gyp.main(args)
  if rc != 0:
    print('Error running GYP')
    sys.exit(rc)


if __name__ == '__main__':
  run_gyp(sys.argv[1:])
