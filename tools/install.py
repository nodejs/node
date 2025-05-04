#!/usr/bin/env python3

import argparse
import ast
import errno
import os
import shutil
import sys

def abspath(*args):
  return os.path.abspath(os.path.join(*args))

def try_mkdir_r(path):
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST: raise

def try_unlink(path):
  try:
    os.unlink(path)
  except OSError as e:
    if e.errno != errno.ENOENT: raise

def try_symlink(options, source_path, link_path):
  if not options.silent:
    print(f'symlinking {source_path} -> {link_path}')
  try_unlink(link_path)
  try_mkdir_r(os.path.dirname(link_path))
  os.symlink(source_path, link_path)

def mkpaths(options, path, dest):
  if dest.endswith('/') or dest.endswith('\\'):
    target_path = abspath(options.install_path, dest, os.path.basename(path))
  else:
    target_path = abspath(options.install_path, dest)

  source_path = abspath(options.root_dir, path) if not os.path.isabs(path) else path
  return source_path, target_path

def try_copy(options, path, dest):
  source_path, target_path = mkpaths(options, path, dest)
  if not options.silent:
    print(f'installing {target_path}')
  try_mkdir_r(os.path.dirname(target_path))
  try_unlink(target_path)  # prevent ETXTBSY errors
  return shutil.copy2(source_path, target_path)

def try_remove(options, path, dest):
  _, target_path = mkpaths(options, path, dest)
  if not options.silent:
    print(f'removing {target_path}')
  try_unlink(target_path)

def install(options, paths, dest):
  for path in paths:
    try_copy(options, path, dest)

def uninstall(options, paths, dest):
  for path in paths:
    try_remove(options, path, dest)

def package_files(options, action, name, bins):
  target_path = os.path.join('lib/node_modules', name)

  # Skip if symlink exists (likely dev version)
  if os.path.islink(abspath(options.install_path, target_path)):
    return

  # Process all files in the package
  root = os.path.join('deps', name)
  for dirname, subdirs, basenames in os.walk(root, topdown=True):
    subdirs[:] = [d for d in subdirs if d != 'test']
    paths = [os.path.join(dirname, basename) for basename in basenames]
    action(options, paths,
           os.path.join(target_path, dirname[len(root) + 1:]) + os.path.sep)

  # Handle bin symlinks
  for bin_name, bin_target in bins.items():
    link_path = abspath(options.install_path, os.path.join('bin', bin_name))
    if action == uninstall:
      action(options, [link_path], os.path.join('bin', bin_name))
    elif action == install:
      try_symlink(options, os.path.join('../lib/node_modules', name, bin_target), link_path)

def subdir_files(options, path, dest, action_func):
  source_path, _ = mkpaths(options, path, dest)
  file_map = {}

  for dirpath, _, filenames in os.walk(source_path):
    headers = [os.path.join(os.path.relpath(dirpath, options.root_dir), f)
              for f in filenames if f.endswith('.h')]
    if headers:
      subdir = os.path.join(dest, os.path.relpath(dirpath, source_path))
      file_map[subdir] = headers

  for subdir, files in file_map.items():
    action_func(options, files, subdir + os.path.sep)

def headers(options, action):
  # Core Node.js headers
  action(options, [
    options.config_gypi_path,
    'common.gypi',
    'src/node.h',
    'src/node_api.h',
    'src/js_native_api.h',
    'src/js_native_api_types.h',
    'src/node_api_types.h',
    'src/node_buffer.h',
    'src/node_object_wrap.h',
    'src/node_version.h',
  ], 'include/node/')

  # Handle dependencies' headers based on build configuration
  variables = options.variables

  # V8 headers
  if os.path.exists(options.v8_dir):
    subdir_files(options, os.path.join(options.v8_dir, 'include'), 'include/node/', action)

  # libuv headers
  if variables.get('node_shared_libuv') == 'false':
    subdir_files(options, 'deps/uv/include', 'include/node/', action)

  # OpenSSL headers
  if variables.get('node_use_openssl') == 'true' and variables.get('node_shared_openssl') == 'false':
    subdir_files(options, 'deps/openssl/openssl/include/openssl', 'include/node/openssl/', action)
    subdir_files(options, 'deps/openssl/config/archs', 'include/node/openssl/archs', action)
    subdir_files(options, 'deps/openssl/config', 'include/node/openssl', action)

  # zlib headers
  if variables.get('node_shared_zlib') == 'false':
    action(options, [
      'deps/zlib/zconf.h',
      'deps/zlib/zlib.h',
    ], 'include/node/')

def files(options, action):
  is_win = options.is_win
  node_bin = 'node.exe' if is_win else 'node'

  # Install Node binary
  action(options, [os.path.join(options.build_dir, node_bin)],
         os.path.join('bin', node_bin))

  # Handle shared library if built
  if options.variables.get('node_shared') == 'true':
    if is_win:
      action(options, [os.path.join(options.build_dir, 'libnode.dll')], 'bin/libnode.dll')
      action(options, [os.path.join(options.build_dir, 'libnode.lib')], 'lib/libnode.lib')
    else:
      lib_suffix = options.variables.get('shlib_suffix')
      output_lib = f'libnode.{lib_suffix}'
      action(options, [os.path.join(options.build_dir, output_lib)],
             os.path.join(options.variables.get('libdir', 'lib'), output_lib))

  # Documentation
  action(options, [os.path.join(options.v8_dir, 'tools/gdbinit')], 'share/doc/node/')
  action(options, [os.path.join(options.v8_dir, 'tools/lldb_commands.py')], 'share/doc/node/')

  # Man pages
  man_path = 'man/man1/' if 'openbsd' in sys.platform else 'share/man/man1/'
  action(options, ['doc/node.1'], man_path)

  # Optional components
  if options.variables.get('node_install_npm') == 'true':
    package_files(options, action, 'npm', {
      'npm': 'bin/npm-cli.js',
      'npx': 'bin/npx-cli.js',
    })

  if options.variables.get('node_install_corepack') == 'true':
    package_files(options, action, 'corepack', {
      'corepack': 'dist/corepack.js',
    })

  # Install headers
  headers(options, action)

def parse_options():
  parser = argparse.ArgumentParser(description='Install Node.js headers and binaries')
  parser.add_argument('command', choices=['install', 'uninstall'])
  parser.add_argument('--dest-dir', default=os.getcwd(), help='custom install prefix (DESTDIR)')
  parser.add_argument('--prefix', default='/usr/local', help='custom install prefix')
  parser.add_argument('--headers-only', action='store_true', help='only install headers')
  parser.add_argument('--root-dir', default=os.getcwd(), help='source code root directory')
  parser.add_argument('--build-dir', default='out/Release', help='location of built binaries')
  parser.add_argument('--v8-dir', default='deps/v8', help='location of V8')
  parser.add_argument('--config-gypi-path', default='config.gypi', help='location of config.gypi')
  parser.add_argument('--is-win', action='store_true',
                     default=(sys.platform in ['win32', 'cygwin']), help='build for Windows')
  parser.add_argument('--silent', action='store_true', help='suppress output logs')

  options = parser.parse_args()
  options.install_path = os.path.join(options.dest_dir + options.prefix)

  # Read build variables from config.gypi
  with open(options.config_gypi_path) as f:
    options.variables = ast.literal_eval(f.read())['variables']

  return options

def main():
  options = parse_options()

  if options.headers_only:
    if options.command == 'install':
      headers(options, install)
    elif options.command == 'uninstall':
      headers(options, uninstall)
  else:
    if options.command == 'install':
      files(options, install)
    elif options.command == 'uninstall':
      files(options, uninstall)
    else:
      raise RuntimeError(f'Invalid command: {options.command}')

if __name__ == '__main__':
  main()
