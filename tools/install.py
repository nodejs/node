#!/usr/bin/env python

import errno
import json
import os
import re
import shutil
import sys

# set at init time
dst_dir = None
node_prefix = None # dst_dir without DESTDIR prefix
target_defaults = None
variables = None

def abspath(*args):
  path = os.path.join(*args)
  return os.path.abspath(path)

def load_config():
  s = open('config.gypi').read()
  s = re.sub(r'#.*?\n', '', s) # strip comments
  s = re.sub(r'\'', '"', s) # convert quotes
  return json.loads(s)

def try_unlink(path):
  try:
    os.unlink(path)
  except OSError, e:
    if e.errno != errno.ENOENT: raise

def try_symlink(source_path, link_path):
  print 'symlinking %s -> %s' % (source_path, link_path)
  try_unlink(link_path)
  os.symlink(source_path, link_path)

def try_mkdir_r(path):
  try:
    os.makedirs(path)
  except OSError, e:
    if e.errno != errno.EEXIST: raise

def try_rmdir_r(path):
  path = abspath(path)
  while path.startswith(dst_dir):
    try:
      os.rmdir(path)
    except OSError, e:
      if e.errno == errno.ENOTEMPTY: return
      if e.errno == errno.ENOENT: return
      raise
    path = abspath(path, '..')

def mkpaths(path, dst):
  if dst.endswith('/'):
    target_path = abspath(dst_dir, dst, os.path.basename(path))
  else:
    target_path = abspath(dst_dir, dst)
  return path, target_path

def try_copy(path, dst):
  source_path, target_path = mkpaths(path, dst)
  print 'installing %s' % target_path
  try_mkdir_r(os.path.dirname(target_path))
  try_unlink(target_path) # prevent ETXTBSY errors
  return shutil.copy2(source_path, target_path)

def try_remove(path, dst):
  source_path, target_path = mkpaths(path, dst)
  print 'removing %s' % target_path
  try_unlink(target_path)
  try_rmdir_r(os.path.dirname(target_path))

def install(paths, dst): map(lambda path: try_copy(path, dst), paths)
def uninstall(paths, dst): map(lambda path: try_remove(path, dst), paths)

def waf_files(action):
  action(['tools/node-waf'], 'bin/node-waf')
  action(['tools/wafadmin/ansiterm.py',
          'tools/wafadmin/Build.py',
          'tools/wafadmin/Configure.py',
          'tools/wafadmin/Constants.py',
          'tools/wafadmin/Environment.py',
          'tools/wafadmin/__init__.py',
          'tools/wafadmin/Logs.py',
          'tools/wafadmin/Node.py',
          'tools/wafadmin/Options.py',
          'tools/wafadmin/pproc.py',
          'tools/wafadmin/py3kfixes.py',
          'tools/wafadmin/Runner.py',
          'tools/wafadmin/Scripting.py',
          'tools/wafadmin/TaskGen.py',
          'tools/wafadmin/Task.py',
          'tools/wafadmin/Utils.py'],
          'lib/node/wafadmin/')
  action(['tools/wafadmin/Tools/ar.py',
          'tools/wafadmin/Tools/cc.py',
          'tools/wafadmin/Tools/ccroot.py',
          'tools/wafadmin/Tools/compiler_cc.py',
          'tools/wafadmin/Tools/compiler_cxx.py',
          'tools/wafadmin/Tools/compiler_d.py',
          'tools/wafadmin/Tools/config_c.py',
          'tools/wafadmin/Tools/cxx.py',
          'tools/wafadmin/Tools/dmd.py',
          'tools/wafadmin/Tools/d.py',
          'tools/wafadmin/Tools/gas.py',
          'tools/wafadmin/Tools/gcc.py',
          'tools/wafadmin/Tools/gdc.py',
          'tools/wafadmin/Tools/gnu_dirs.py',
          'tools/wafadmin/Tools/gob2.py',
          'tools/wafadmin/Tools/gxx.py',
          'tools/wafadmin/Tools/icc.py',
          'tools/wafadmin/Tools/icpc.py',
          'tools/wafadmin/Tools/__init__.py',
          'tools/wafadmin/Tools/intltool.py',
          'tools/wafadmin/Tools/libtool.py',
          'tools/wafadmin/Tools/misc.py',
          'tools/wafadmin/Tools/nasm.py',
          'tools/wafadmin/Tools/node_addon.py',
          'tools/wafadmin/Tools/osx.py',
          'tools/wafadmin/Tools/preproc.py',
          'tools/wafadmin/Tools/python.py',
          'tools/wafadmin/Tools/suncc.py',
          'tools/wafadmin/Tools/suncxx.py',
          'tools/wafadmin/Tools/unittestw.py',
          'tools/wafadmin/Tools/winres.py',
          'tools/wafadmin/Tools/xlc.py',
          'tools/wafadmin/Tools/xlcxx.py'],
          'lib/node/wafadmin/Tools/')

def update_shebang(path, shebang):
  print 'updating shebang of %s to %s' % (path, shebang)
  s = open(path, 'r').read()
  s = re.sub(r'#!.*\n', '#!' + shebang + '\n', s)
  open(path, 'w').write(s)

def npm_files(action):
  target_path = 'lib/node_modules/npm/'

  # don't install npm if the target path is a symlink, it probably means
  # that a dev version of npm is installed there
  if os.path.islink(abspath(dst_dir, target_path)): return

  # npm has a *lot* of files and it'd be a pain to maintain a fixed list here
  # so we walk its source directory instead...
  for dirname, subdirs, basenames in os.walk('deps/npm', topdown=True):
    subdirs[:] = filter('test'.__ne__, subdirs) # skip test suites
    paths = [os.path.join(dirname, basename) for basename in basenames]
    action(paths, target_path + dirname[9:] + '/')

  # create/remove symlink
  link_path = abspath(dst_dir, 'bin/npm')
  if action == uninstall:
    action([link_path], 'bin/npm')
  elif action == install:
    try_symlink('../lib/node_modules/npm/bin/npm-cli.js', link_path)
    if os.environ.get('PORTABLE'):
      # This crazy hack is necessary to make the shebang execute the copy
      # of node relative to the same directory as the npm script. The precompiled
      # binary tarballs use a prefix of "/" which gets translated to "/bin/node"
      # in the regular shebang modifying logic, which is incorrect since the
      # precompiled bundle should be able to be extracted anywhere and "just work"
      shebang = '/bin/sh\n// 2>/dev/null; exec "`dirname "$0"`/node" "$0" "$@"'
    else:
      shebang = os.path.join(node_prefix, 'bin/node')
    update_shebang(link_path, shebang)
  else:
    assert(0) # unhandled action type

def files(action):
  action(['deps/uv/include/ares.h',
          'deps/uv/include/ares_version.h',
          'deps/uv/include/uv.h',
          'deps/v8/include/v8-debug.h',
          'deps/v8/include/v8-preparser.h',
          'deps/v8/include/v8-profiler.h',
          'deps/v8/include/v8-testing.h',
          'deps/v8/include/v8.h',
          'deps/v8/include/v8stdint.h',
          'src/eio-emul.h',
          'src/ev-emul.h',
          'src/node.h',
          'src/node_buffer.h',
          'src/node_object_wrap.h',
          'src/node_version.h'],
          'include/node/')
  action(['deps/uv/include/uv-private/eio.h',
          'deps/uv/include/uv-private/ev.h',
          'deps/uv/include/uv-private/ngx-queue.h',
          'deps/uv/include/uv-private/tree.h',
          'deps/uv/include/uv-private/uv-unix.h',
          'deps/uv/include/uv-private/uv-win.h'],
          'include/node/uv-private/')
  action(['out/Release/node'], 'bin/node')

  # install unconditionally, checking if the platform supports dtrace doesn't
  # work when cross-compiling and besides, there's at least one linux flavor
  # with dtrace support now (oracle's "unbreakable" linux)
  action(['src/node.d'], 'lib/dtrace/')

  if 'freebsd' in sys.platform or 'openbsd' in sys.platform:
    action(['doc/node.1'], 'man/man1/')
  else:
    action(['doc/node.1'], 'share/man/man1/')

  if 'true' == variables.get('node_install_waf'): waf_files(action)
  if 'true' == variables.get('node_install_npm'): npm_files(action)

def run(args):
  global dst_dir, node_prefix, target_defaults, variables

  # chdir to the project's top-level directory
  os.chdir(abspath(os.path.dirname(__file__), '..'))

  conf = load_config()
  variables = conf['variables']
  target_defaults = conf['target_defaults']

  # argv[2] is a custom install prefix for packagers (think DESTDIR)
  dst_dir = node_prefix = variables.get('node_prefix') or '/usr/local'
  if len(args) > 2: dst_dir = abspath(args[2] + '/' + dst_dir)

  cmd = args[1] if len(args) > 1 else 'install'
  if cmd == 'install': return files(install)
  if cmd == 'uninstall': return files(uninstall)
  raise RuntimeError('Bad command: %s\n' % cmd)

if __name__ == '__main__':
  run(sys.argv[:])
