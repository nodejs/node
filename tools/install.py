#!/usr/bin/env python3

import argparse
import ast
import errno
import os
import platform
import shutil
import sys
import re
from pathlib import Path

current_system = platform.system()
SYSTEM_AIX = "AIX"

def abspath(*args):
  return Path(os.path.join(*args)).absolute()

def is_child_dir(child, parent):
  child_abs = child.absolute()
  parent_abs = parent.absolute()
  return str(child_abs).startswith(str(parent_abs)) and child_abs != parent_abs

def try_unlink(path):
  try:
    path.unlink()
  except FileNotFoundError:
    pass

def try_symlink(options, source_path, link_path):
  if not options.silent:
    print(f'symlinking {source_path} -> {link_path}')
  try_unlink(link_path)
  link_path.parent.mkdir(parents=True, exist_ok=True)
  os.symlink(source_path, link_path)

def try_mkdir_r(path):
  path.mkdir(parents=True, exist_ok=True)

def try_rmdir_r(options, path):
  path = path.absolute()
  install_path = Path(options.install_path)

  while is_child_dir(path, install_path):
    try:
      path.rmdir()
    except OSError as e:
      if e.errno in (errno.ENOTEMPTY, errno.ENOENT) or \
         (e.errno == errno.EEXIST and current_system == SYSTEM_AIX):
        return
      raise
    path = path.parent

def mkpaths(options, path, dest):
  install_path = Path(options.install_path)
  if dest.endswith(('/', '\\')):
    target_path = install_path / dest / Path(path).name
  else:
    target_path = install_path / dest

  source_path = Path(path) if os.path.isabs(path) else Path(options.root_dir) / path
  return source_path, target_path

def try_copy(options, path, dest):
  source_path, target_path = mkpaths(options, path, dest)
  if not options.silent:
    print(f'installing {target_path}')
  target_path.parent.mkdir(parents=True, exist_ok=True)
  try_unlink(target_path)  # prevent ETXTBSY errors
  return Path(shutil.copy2(source_path, target_path))

def try_remove(options, path, dest):
  _, target_path = mkpaths(options, path, dest)
  if not options.silent:
    print(f'removing {target_path}')
  try_unlink(target_path)
  try_rmdir_r(options, target_path.parent)

def install(options, paths, dest):
  for path in paths:
    try_copy(options, path, dest)

def uninstall(options, paths, dest):
  for path in paths:
    try_remove(options, path, dest)

def package_files(options, action, name, bins):
  target_path = Path('lib/node_modules') / name
  install_path = Path(options.install_path)

  # Don't install if target path is a symlink (likely a dev version)
  if (install_path / target_path).is_symlink():
    return

  # Process all files in the package directory
  root = Path('deps') / name
  for dirname, subdirs, basenames in os.walk(root, topdown=True):
    # Skip test directories
    subdirs[:] = [subdir for subdir in subdirs if subdir != 'test']

    # Get paths relative to root
    rel_dir = Path(dirname).relative_to(root) if dirname != str(root) else Path('.')
    paths = [Path(dirname) / basename for basename in basenames]
    target_dir = target_path / rel_dir

    action(options, [str(p) for p in paths], f"{target_dir}/")

  # Create/remove symlinks
  for bin_name, bin_target in bins.items():
    link_path = install_path / 'bin' / bin_name
    if action == uninstall:
      action(options, [str(link_path)], f'bin/{bin_name}')
    elif action == install:
      rel_path = Path('..') / 'lib' / 'node_modules' / name / bin_target
      try_symlink(options, rel_path, link_path)

def npm_files(options, action):
  package_files(options, action, 'npm', {
    'npm': 'bin/npm-cli.js',
    'npx': 'bin/npx-cli.js',
  })

def corepack_files(options, action):
  package_files(options, action, 'corepack', {
    'corepack': 'dist/corepack.js',
#   Not the default just yet:
#   'yarn': 'dist/yarn.js',
#   'yarnpkg': 'dist/yarn.js',
#   'pnpm': 'dist/pnpm.js',
#   'pnpx': 'dist/pnpx.js',
  })

  # On z/OS, install node-gyp for convenience
  if sys.platform == 'zos':
    install_path = Path(options.install_path)
    link_path = install_path / 'bin' / 'node-gyp'
    if action == uninstall:
      action(options, [str(link_path)], 'bin/node-gyp')
    elif action == install:
      try_symlink(options, Path('../lib/node_modules/npm/node_modules/node-gyp/bin/node-gyp.js'), link_path)

def subdir_files(options, path, dest, action):
  source_path, _ = mkpaths(options, path, dest)
  file_map = {}

  for dirpath, _, filenames in os.walk(source_path):
    rel_path = Path(dirpath).relative_to(Path(options.root_dir))
    header_files = [str(rel_path / f) for f in filenames if f.endswith('.h')]
    if header_files:
      rel_to_source = Path(dirpath).relative_to(source_path)
      dest_subdir = Path(dest) / rel_to_source
      file_map[str(dest_subdir)] = header_files

  for subdir, files_in_path in file_map.items():
    action(options, files_in_path, f"{subdir}/")

def files(options, action):
  # Install node binary
  node_bin = 'node.exe' if options.is_win else 'node'
  action(options, [str(Path(options.build_dir) / node_bin)], f'bin/{node_bin}')

  # Handle shared library if needed
  if options.variables.get('node_shared') == 'true':
    if options.is_win:
      action(options, [str(Path(options.build_dir) / 'libnode.dll')], 'bin/libnode.dll')
      action(options, [str(Path(options.build_dir) / 'libnode.lib')], 'lib/libnode.lib')
    elif sys.platform == 'zos':
      # Special handling for z/OS platform
      output_prefix = Path(options.build_dir) / 'lib.target'
      output_lib = f"libnode.{options.variables.get('shlib_suffix')}"
      action(options, [str(output_prefix / output_lib)], f'lib/{output_lib}')

      # Create libnode.x referencing libnode.so
      sidedeck_script = Path(os.path.dirname(os.path.realpath(__file__))) / 'zos/modifysidedeck.sh'
      os.system(f'{sidedeck_script} '
                f'{Path(options.install_path) / "lib" / output_lib} '
                f'{Path(options.install_path) / "lib/libnode.x"} libnode.so')

      # Install libnode.version.so
      so_name = f"libnode.{re.sub(r'\.x$', '.so', options.variables.get('shlib_suffix'))}"
      action(options, [str(output_prefix / so_name)], f"{options.variables.get('libdir')}/{so_name}")

      # Create symlink of libnode.so -> libnode.version.so
      link_path = Path(options.install_path) / 'lib/libnode.so'
      try_symlink(options, Path(so_name), link_path)
    else:
      # Standard shared library handling for other platforms
      output_lib = f"libnode.{options.variables.get('shlib_suffix')}"
      libdir = options.variables.get('libdir')
      action(options, [str(Path(options.build_dir) / output_lib)], f'{libdir}/{output_lib}')

  action(options, [str(Path(options.v8_dir) / 'tools/gdbinit')], 'share/doc/node/')
  action(options, [str(Path(options.v8_dir) / 'tools/lldb_commands.py')], 'share/doc/node/')

  man_dir = 'man/man1/' if 'openbsd' in sys.platform else 'share/man/man1/'
  action(options, ['doc/node.1'], man_dir)

  if options.variables.get('node_install_npm') == 'true':
    npm_files(options, action)

  if options.variables.get('node_install_corepack') == 'true':
    corepack_files(options, action)

  headers(options, action)

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

  # Add the expfile that is created on AIX
  if sys.platform.startswith('aix') or sys.platform == "os400":
    action(options, ['out/Release/node.exp'], 'include/node/')

  # V8 headers
  def wanted_v8_headers(options, files_arg, dest):
    v8_headers = {
      # The internal cppgc headers are depended on by the public
      # ones, so they need to be included as well.
      'include/cppgc/internal/api-constants.h',
      'include/cppgc/internal/atomic-entry-flag.h',
      'include/cppgc/internal/base-page-handle.h',
      'include/cppgc/internal/caged-heap-local-data.h',
      'include/cppgc/internal/caged-heap.h',
      'include/cppgc/internal/compiler-specific.h',
      'include/cppgc/internal/conditional-stack-allocated.h',
      'include/cppgc/internal/finalizer-trait.h',
      'include/cppgc/internal/gc-info.h',
      'include/cppgc/internal/logging.h',
      'include/cppgc/internal/member-storage.h',
      'include/cppgc/internal/name-trait.h',
      'include/cppgc/internal/persistent-node.h',
      'include/cppgc/internal/pointer-policies.h',
      'include/cppgc/internal/write-barrier.h',
      # cppgc headers
      'include/cppgc/allocation.h',
      'include/cppgc/common.h',
      'include/cppgc/cross-thread-persistent.h',
      'include/cppgc/custom-space.h',
      'include/cppgc/default-platform.h',
      'include/cppgc/explicit-management.h',
      'include/cppgc/garbage-collected.h',
      'include/cppgc/heap-consistency.h',
      'include/cppgc/heap-handle.h',
      'include/cppgc/heap-state.h',
      'include/cppgc/heap-statistics.h',
      'include/cppgc/heap.h',
      'include/cppgc/liveness-broker.h',
      'include/cppgc/macros.h',
      'include/cppgc/member.h',
      'include/cppgc/name-provider.h',
      'include/cppgc/object-size-trait.h',
      'include/cppgc/persistent.h',
      'include/cppgc/platform.h',
      'include/cppgc/prefinalizer.h',
      'include/cppgc/process-heap-statistics.h',
      'include/cppgc/sentinel-pointer.h',
      'include/cppgc/source-location.h',
      'include/cppgc/testing.h',
      'include/cppgc/trace-trait.h',
      'include/cppgc/type-traits.h',
      'include/cppgc/visitor.h',
      # libplatform headers
      'include/libplatform/libplatform-export.h',
      'include/libplatform/libplatform.h',
      'include/libplatform/v8-tracing.h',
      # v8 headers
      'include/v8-array-buffer.h',
      'include/v8-callbacks.h',
      'include/v8-container.h',
      'include/v8-context.h',
      'include/v8-cppgc.h',
      'include/v8-data.h',
      'include/v8-date.h',
      'include/v8-debug.h',
      'include/v8-embedder-heap.h',
      'include/v8-embedder-state-scope.h',
      'include/v8-exception.h',
      'include/v8-extension.h',
      'include/v8-external.h',
      'include/v8-forward.h',
      'include/v8-function-callback.h',
      'include/v8-function.h',
      'include/v8-handle-base.h',
      'include/v8-initialization.h',
      'include/v8-internal.h',
      'include/v8-isolate.h',
      'include/v8-json.h',
      'include/v8-local-handle.h',
      'include/v8-locker.h',
      'include/v8-maybe.h',
      'include/v8-memory-span.h',
      'include/v8-message.h',
      'include/v8-microtask-queue.h',
      'include/v8-microtask.h',
      'include/v8-object.h',
      'include/v8-persistent-handle.h',
      'include/v8-platform.h',
      'include/v8-primitive-object.h',
      'include/v8-primitive.h',
      'include/v8-profiler.h',
      'include/v8-promise.h',
      'include/v8-proxy.h',
      'include/v8-regexp.h',
      'include/v8-sandbox.h',
      'include/v8-script.h',
      'include/v8-snapshot.h',
      'include/v8-source-location.h',
      'include/v8-statistics.h',
      'include/v8-template.h',
      'include/v8-traced-handle.h',
      'include/v8-typed-array.h',
      'include/v8-unwinder.h',
      'include/v8-value-serializer.h',
      'include/v8-value.h',
      'include/v8-version.h',
      'include/v8-wasm.h',
      'include/v8-weak-callback-info.h',
      'include/v8.h',
      'include/v8config.h',
    }

    if sys.platform == 'win32':
      v8_headers = {os.path.normpath(path) for path in v8_headers}

    if os.path.isabs(options.v8_dir):
      rel_v8_dir = Path(options.v8_dir).relative_to(Path(options.root_dir))
    else:
      rel_v8_dir = Path(options.v8_dir)

    # Filter files to include only the wanted V8 headers
    filtered_files = [
      name for name in files_arg
      if Path(name).relative_to(rel_v8_dir).as_posix() in v8_headers
    ]

    action(options, filtered_files, dest)

  def wanted_zoslib_headers(options, files_arg, dest):
    import glob
    zoslib_headers = set(glob.glob(f"{zoslibinc}/*.h"))
    filtered_files = [name for name in files_arg if name in zoslib_headers]
    action(options, filtered_files, dest)

  subdir_files(options, os.path.join(options.v8_dir, 'include'), 'include/node/', wanted_v8_headers)

  if options.variables.get('node_shared_libuv') == 'false':
    subdir_files(options, 'deps/uv/include', 'include/node/', action)

  if (options.variables.get('node_use_openssl') == 'true' and
      options.variables.get('node_shared_openssl') == 'false'):
    subdir_files(options, 'deps/openssl/openssl/include/openssl', 'include/node/openssl/', action)
    subdir_files(options, 'deps/openssl/config/archs', 'include/node/openssl/archs', action)
    subdir_files(options, 'deps/openssl/config', 'include/node/openssl', action)

  if options.variables.get('node_shared_zlib') == 'false':
    action(options, [
      'deps/zlib/zconf.h',
      'deps/zlib/zlib.h',
    ], 'include/node/')

  # Handle z/OS specific libraries
  if sys.platform == 'zos':
    zoslibinc = os.environ.get('ZOSLIB_INCLUDES')
    if not zoslibinc:
      raise RuntimeError('Environment variable ZOSLIB_INCLUDES is not set')
    if not os.path.isfile(os.path.join(zoslibinc, 'zos-base.h')):
      raise RuntimeError('ZOSLIB_INCLUDES is not set to a valid location')
    subdir_files(options, zoslibinc, 'include/node/zoslib/', wanted_zoslib_headers)

def run(options):
  if options.headers_only:
    action = install if options.command == 'install' else uninstall
    headers(options, action)
  else:
    action = install if options.command == 'install' else uninstall
    files(options, action)

def parse_options(args):
  parser = argparse.ArgumentParser(
    description='Install headers and binaries into filesystem')
  parser.add_argument('command', choices=['install', 'uninstall'])
  parser.add_argument('--dest-dir',
                      help='custom install prefix for packagers, i.e. DESTDIR',
                      default=os.getcwd())
  parser.add_argument('--prefix',
                      help='custom install prefix, i.e. PREFIX',
                      default='/usr/local')
  parser.add_argument('--headers-only',
                      help='only install headers',
                      action='store_true', default=False)
  parser.add_argument('--root-dir',
                      help='the root directory of source code',
                      default=os.getcwd())
  parser.add_argument('--build-dir',
                      help='the location of built binaries',
                      default='out/Release')
  parser.add_argument('--v8-dir',
                      help='the location of V8',
                      default='deps/v8')
  parser.add_argument('--config-gypi-path',
                      help='the location of config.gypi',
                      default='config.gypi')
  parser.add_argument('--is-win',
                      help='build for Windows target',
                      action='store_true',
                      default=(sys.platform in ['win32', 'cygwin']))
  parser.add_argument('--silent',
                      help='do not output log',
                      action='store_true', default=False)
  options = parser.parse_args(args)

  # |dest_dir| is a custom install prefix for packagers (think DESTDIR)
  # |prefix| is a custom install prefix (think PREFIX)
  # Difference is that dest_dir won't be included in shebang lines etc.
  # |install_path| thus becomes the base target directory.
  options.install_path = os.path.join(options.dest_dir + options.prefix)

  # Read variables from the config.gypi.
  with open(options.config_gypi_path) as f:
    config = ast.literal_eval(f.read())
    options.variables = config['variables']

  return options

if __name__ == '__main__':
  run(parse_options(sys.argv[1:]))
