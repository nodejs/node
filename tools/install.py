#!/usr/bin/env python

import errno

try:
  import json
except ImportError:
  import simplejson as json

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
  action(['out/Release/node'], 'bin/node')

  # install unconditionally, checking if the platform supports dtrace doesn't
  # work when cross-compiling and besides, there's at least one linux flavor
  # with dtrace support now (oracle's "unbreakable" linux)
  action(['src/node.d'], 'lib/dtrace/')

  if 'freebsd' in sys.platform or 'openbsd' in sys.platform:
    action(['doc/node.1'], 'man/man1/')
  else:
    action(['doc/node.1'], 'share/man/man1/')

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
