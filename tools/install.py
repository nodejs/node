#!/usr/bin/env python

from __future__ import print_function

import ast
import errno
import os
import shutil
import sys
import re

# set at init time
node_prefix = '/usr/local' # PREFIX variable from Makefile
install_path = '' # base target directory (DESTDIR + PREFIX from Makefile)
target_defaults = None
variables = None

def abspath(*args):
  path = os.path.join(*args)
  return os.path.abspath(path)

def load_config():
  with open('config.gypi') as f:
    return ast.literal_eval(f.read())

def try_unlink(path):
  try:
    os.unlink(path)
  except OSError as e:
    if e.errno != errno.ENOENT: raise

def try_symlink(source_path, link_path):
  print('symlinking %s -> %s' % (source_path, link_path))
  try_unlink(link_path)
  try_mkdir_r(os.path.dirname(link_path))
  os.symlink(source_path, link_path)

def try_mkdir_r(path):
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST: raise

def try_rmdir_r(path):
  path = abspath(path)
  while path.startswith(install_path):
    try:
      os.rmdir(path)
    except OSError as e:
      if e.errno == errno.ENOTEMPTY: return
      if e.errno == errno.ENOENT: return
      raise
    path = abspath(path, '..')

def mkpaths(path, dst):
  if dst.endswith('/'):
    target_path = abspath(install_path, dst, os.path.basename(path))
  else:
    target_path = abspath(install_path, dst)
  return path, target_path

def try_copy(path, dst):
  source_path, target_path = mkpaths(path, dst)
  print('installing %s' % target_path)
  try_mkdir_r(os.path.dirname(target_path))
  try_unlink(target_path) # prevent ETXTBSY errors
  return shutil.copy2(source_path, target_path)

def try_remove(path, dst):
  source_path, target_path = mkpaths(path, dst)
  print('removing %s' % target_path)
  try_unlink(target_path)
  try_rmdir_r(os.path.dirname(target_path))

def install(paths, dst):
  for path in paths:
    try_copy(path, dst)

def uninstall(paths, dst):
  for path in paths:
    try_remove(path, dst)

def package_files(action, name, bins):
  target_path = 'lib/node_modules/' + name + '/'

  # don't install npm if the target path is a symlink, it probably means
  # that a dev version of npm is installed there
  if os.path.islink(abspath(install_path, target_path)): return

  # npm has a *lot* of files and it'd be a pain to maintain a fixed list here
  # so we walk its source directory instead...
  root = 'deps/' + name
  for dirname, subdirs, basenames in os.walk(root, topdown=True):
    subdirs[:] = [subdir for subdir in subdirs if subdir != 'test']
    paths = [os.path.join(dirname, basename) for basename in basenames]
    action(paths, target_path + dirname[len(root) + 1:] + '/')

  # create/remove symlinks
  for bin_name, bin_target in bins.items():
    link_path = abspath(install_path, 'bin/' + bin_name)
    if action == uninstall:
      action([link_path], 'bin/' + bin_name)
    elif action == install:
      try_symlink('../lib/node_modules/' + name + '/' + bin_target, link_path)
    else:
      assert 0  # unhandled action type

def npm_files(action):
  package_files(action, 'npm', {
    'npm': 'bin/npm-cli.js',
    'npx': 'bin/npx-cli.js',
  })

def corepack_files(action):
  package_files(action, 'corepack', {
    'corepack': 'dist/corepack.js',
#   Not the default just yet:
#   'yarn': 'dist/yarn.js',
#   'yarnpkg': 'dist/yarn.js',
#   'pnpm': 'dist/pnpm.js',
#   'pnpx': 'dist/pnpx.js',
  })

  # On z/OS, we install node-gyp for convenience, as some vendors don't have
  # external access and may want to build native addons.
  if sys.platform == 'zos':
    link_path = abspath(install_path, 'bin/node-gyp')
    if action == uninstall:
      action([link_path], 'bin/node-gyp')
    elif action == install:
      try_symlink('../lib/node_modules/npm/node_modules/node-gyp/bin/node-gyp.js', link_path)
    else:
      assert 0 # unhandled action type

def subdir_files(path, dest, action):
  ret = {}
  for dirpath, dirnames, filenames in os.walk(path):
    files_in_path = [dirpath + '/' + f for f in filenames if f.endswith('.h')]
    ret[dest + dirpath.replace(path, '')] = files_in_path
  for subdir, files_in_path in ret.items():
    action(files_in_path, subdir + '/')

def files(action):
  is_windows = sys.platform == 'win32'
  output_file = 'node'
  output_prefix = 'out/Release/'

  if is_windows:
    output_file += '.exe'
  action([output_prefix + output_file], 'bin/' + output_file)

  if 'true' == variables.get('node_shared'):
    if is_windows:
      action([output_prefix + 'libnode.dll'], 'bin/libnode.dll')
      action([output_prefix + 'libnode.lib'], 'lib/libnode.lib')
    elif sys.platform == 'zos':
      # GYP will output to lib.target; see _InstallableTargetInstallPath
      # function in tools/gyp/pylib/gyp/generator/make.py
      output_prefix += 'lib.target/'

      output_lib = 'libnode.' + variables.get('shlib_suffix')
      action([output_prefix + output_lib], 'lib/' + output_lib)

      # create libnode.x that references libnode.so (C++ addons compat)
      os.system(os.path.dirname(os.path.realpath(__file__)) +
                '/zos/modifysidedeck.sh ' +
                abspath(install_path, 'lib/' + output_lib) + ' ' +
                abspath(install_path, 'lib/libnode.x') + ' libnode.so')

      # install libnode.version.so
      so_name = 'libnode.' + re.sub(r'\.x$', '.so', variables.get('shlib_suffix'))
      action([output_prefix + so_name], variables.get('libdir') + '/' + so_name)

      # create symlink of libnode.so -> libnode.version.so (C++ addons compat)
      link_path = abspath(install_path, 'lib/libnode.so')
      try_symlink(so_name, link_path)
    else:
      output_lib = 'libnode.' + variables.get('shlib_suffix')
      action([output_prefix + output_lib], variables.get('libdir') + '/' + output_lib)

  action(['deps/v8/tools/gdbinit'], 'share/doc/node/')
  action(['deps/v8/tools/lldb_commands.py'], 'share/doc/node/')

  if 'freebsd' in sys.platform or 'openbsd' in sys.platform:
    action(['doc/node.1'], 'man/man1/')
  else:
    action(['doc/node.1'], 'share/man/man1/')

  if 'true' == variables.get('node_install_npm'):
    npm_files(action)

  if 'true' == variables.get('node_install_corepack'):
    corepack_files(action)

  headers(action)

def headers(action):
  def wanted_v8_headers(files_arg, dest):
    v8_headers = [
      # The internal cppgc headers are depended on by the public
      # ones, so they need to be included as well.
      'deps/v8/include/cppgc/internal/api-constants.h',
      'deps/v8/include/cppgc/internal/atomic-entry-flag.h',
      'deps/v8/include/cppgc/internal/base-page-handle.h',
      'deps/v8/include/cppgc/internal/caged-heap-local-data.h',
      'deps/v8/include/cppgc/internal/caged-heap.h',
      'deps/v8/include/cppgc/internal/compiler-specific.h',
      'deps/v8/include/cppgc/internal/finalizer-trait.h',
      'deps/v8/include/cppgc/internal/gc-info.h',
      'deps/v8/include/cppgc/internal/logging.h',
      'deps/v8/include/cppgc/internal/member-storage.h',
      'deps/v8/include/cppgc/internal/name-trait.h',
      'deps/v8/include/cppgc/internal/persistent-node.h',
      'deps/v8/include/cppgc/internal/pointer-policies.h',
      'deps/v8/include/cppgc/internal/write-barrier.h',
      # cppgc headers
      'deps/v8/include/cppgc/allocation.h',
      'deps/v8/include/cppgc/common.h',
      'deps/v8/include/cppgc/cross-thread-persistent.h',
      'deps/v8/include/cppgc/custom-space.h',
      'deps/v8/include/cppgc/default-platform.h',
      'deps/v8/include/cppgc/ephemeron-pair.h',
      'deps/v8/include/cppgc/explicit-management.h',
      'deps/v8/include/cppgc/garbage-collected.h',
      'deps/v8/include/cppgc/heap-consistency.h',
      'deps/v8/include/cppgc/heap-handle.h',
      'deps/v8/include/cppgc/heap-state.h',
      'deps/v8/include/cppgc/heap-statistics.h',
      'deps/v8/include/cppgc/heap.h',
      'deps/v8/include/cppgc/liveness-broker.h',
      'deps/v8/include/cppgc/macros.h',
      'deps/v8/include/cppgc/member.h',
      'deps/v8/include/cppgc/name-provider.h',
      'deps/v8/include/cppgc/object-size-trait.h',
      'deps/v8/include/cppgc/persistent.h',
      'deps/v8/include/cppgc/platform.h',
      'deps/v8/include/cppgc/prefinalizer.h',
      'deps/v8/include/cppgc/process-heap-statistics.h',
      'deps/v8/include/cppgc/sentinel-pointer.h',
      'deps/v8/include/cppgc/source-location.h',
      'deps/v8/include/cppgc/testing.h',
      'deps/v8/include/cppgc/trace-trait.h',
      'deps/v8/include/cppgc/type-traits.h',
      'deps/v8/include/cppgc/visitor.h',
      # libplatform headers
      'deps/v8/include/libplatform/libplatform-export.h',
      'deps/v8/include/libplatform/libplatform.h',
      'deps/v8/include/libplatform/v8-tracing.h',
      # v8 headers
      'deps/v8/include/v8-array-buffer.h',
      'deps/v8/include/v8-callbacks.h',
      'deps/v8/include/v8-container.h',
      'deps/v8/include/v8-context.h',
      'deps/v8/include/v8-cppgc.h',
      'deps/v8/include/v8-data.h',
      'deps/v8/include/v8-date.h',
      'deps/v8/include/v8-debug.h',
      'deps/v8/include/v8-embedder-heap.h',
      'deps/v8/include/v8-embedder-state-scope.h',
      'deps/v8/include/v8-exception.h',
      'deps/v8/include/v8-extension.h',
      'deps/v8/include/v8-external.h',
      'deps/v8/include/v8-forward.h',
      'deps/v8/include/v8-function-callback.h',
      'deps/v8/include/v8-function.h',
      'deps/v8/include/v8-initialization.h',
      'deps/v8/include/v8-internal.h',
      'deps/v8/include/v8-isolate.h',
      'deps/v8/include/v8-json.h',
      'deps/v8/include/v8-local-handle.h',
      'deps/v8/include/v8-locker.h',
      'deps/v8/include/v8-maybe.h',
      'deps/v8/include/v8-memory-span.h',
      'deps/v8/include/v8-message.h',
      'deps/v8/include/v8-microtask-queue.h',
      'deps/v8/include/v8-microtask.h',
      'deps/v8/include/v8-object.h',
      'deps/v8/include/v8-persistent-handle.h',
      'deps/v8/include/v8-platform.h',
      'deps/v8/include/v8-primitive-object.h',
      'deps/v8/include/v8-primitive.h',
      'deps/v8/include/v8-profiler.h',
      'deps/v8/include/v8-promise.h',
      'deps/v8/include/v8-proxy.h',
      'deps/v8/include/v8-regexp.h',
      'deps/v8/include/v8-script.h',
      'deps/v8/include/v8-snapshot.h',
      'deps/v8/include/v8-statistics.h',
      'deps/v8/include/v8-template.h',
      'deps/v8/include/v8-traced-handle.h',
      'deps/v8/include/v8-typed-array.h',
      'deps/v8/include/v8-unwinder.h',
      'deps/v8/include/v8-value-serializer.h',
      'deps/v8/include/v8-value.h',
      'deps/v8/include/v8-version.h',
      'deps/v8/include/v8-wasm.h',
      'deps/v8/include/v8-weak-callback-info.h',
      'deps/v8/include/v8.h',
      'deps/v8/include/v8config.h',
    ]
    files_arg = [name for name in files_arg if name in v8_headers]
    action(files_arg, dest)

  def wanted_zoslib_headers(files_arg, dest):
    import glob
    zoslib_headers = glob.glob(zoslibinc + '/*.h')
    files_arg = [name for name in files_arg if name in zoslib_headers]
    action(files_arg, dest)

  action([
    'common.gypi',
    'config.gypi',
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
    action(['out/Release/node.exp'], 'include/node/')

  subdir_files('deps/v8/include', 'include/node/', wanted_v8_headers)

  if 'false' == variables.get('node_shared_libuv'):
    subdir_files('deps/uv/include', 'include/node/', action)

  if 'true' == variables.get('node_use_openssl') and \
     'false' == variables.get('node_shared_openssl'):
    subdir_files('deps/openssl/openssl/include/openssl', 'include/node/openssl/', action)
    subdir_files('deps/openssl/config/archs', 'include/node/openssl/archs', action)
    subdir_files('deps/openssl/config', 'include/node/openssl', action)

  if 'false' == variables.get('node_shared_zlib'):
    action([
      'deps/zlib/zconf.h',
      'deps/zlib/zlib.h',
    ], 'include/node/')

  if sys.platform == 'zos':
    zoslibinc = os.environ.get('ZOSLIB_INCLUDES')
    if not zoslibinc:
      raise RuntimeError('Environment variable ZOSLIB_INCLUDES is not set\n')
    if not os.path.isfile(zoslibinc + '/zos-base.h'):
      raise RuntimeError('ZOSLIB_INCLUDES is not set to a valid location\n')
    subdir_files(zoslibinc, 'include/node/zoslib/', wanted_zoslib_headers)

def run(args):
  global node_prefix, install_path, target_defaults, variables

  # chdir to the project's top-level directory
  os.chdir(abspath(os.path.dirname(__file__), '..'))

  conf = load_config()
  variables = conf['variables']
  target_defaults = conf['target_defaults']

  # argv[2] is a custom install prefix for packagers (think DESTDIR)
  # argv[3] is a custom install prefix (think PREFIX)
  # Difference is that dst_dir won't be included in shebang lines etc.
  dst_dir = args[2] if len(args) > 2 else ''

  if len(args) > 3:
    node_prefix = args[3]

  # install_path thus becomes the base target directory.
  install_path = dst_dir + node_prefix + '/'

  cmd = args[1] if len(args) > 1 else 'install'

  if os.environ.get('HEADERS_ONLY'):
    if cmd == 'install':
      headers(install)
      return
    if cmd == 'uninstall':
      headers(uninstall)
      return
  else:
    if cmd == 'install':
      files(install)
      return
    if cmd == 'uninstall':
      files(uninstall)
      return

  raise RuntimeError('Bad command: %s\n' % cmd)

if __name__ == '__main__':
  run(sys.argv[:])
