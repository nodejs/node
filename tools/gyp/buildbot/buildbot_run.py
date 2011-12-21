#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Argument-less script to select what to run on the buildbots."""


import os
import shutil
import subprocess
import sys


if sys.platform in ['win32', 'cygwin']:
  EXE_SUFFIX = '.exe'
else:
  EXE_SUFFIX = ''


BUILDBOT_DIR = os.path.dirname(os.path.abspath(__file__))
TRUNK_DIR = os.path.dirname(BUILDBOT_DIR)
ROOT_DIR = os.path.dirname(TRUNK_DIR)
OUT_DIR = os.path.join(TRUNK_DIR, 'out')
NINJA_PATH = os.path.join(TRUNK_DIR, 'ninja' + EXE_SUFFIX)
NINJA_WORK_DIR = os.path.join(ROOT_DIR, 'ninja_work')


def InstallNinja():
  """Install + build ninja.

  Returns:
    0 for success, 1 for failure.
  """
  print '@@@BUILD_STEP install ninja@@@'
  # Delete old version if any.
  try:
    shutil.rmtree(NINJA_WORK_DIR, ignore_errors=True)
  except:
    pass
  # Sync new copy from git.
  subprocess.check_call(
      'git clone https://github.com/martine/ninja.git ' + NINJA_WORK_DIR,
      shell=True)
  # Bootstrap.
  subprocess.check_call('./bootstrap.sh', cwd=NINJA_WORK_DIR, shell=True)
  # Copy out ninja.
  shutil.copyfile(os.path.join(NINJA_WORK_DIR, 'ninja' + EXE_SUFFIX),
                  NINJA_PATH)
  os.chmod(NINJA_PATH, 0777)


def GypTestFormat(title, format=None, msvs_version=None):
  """Run the gyp tests for a given format, emitting annotator tags.

  See annotator docs at:
    https://sites.google.com/a/chromium.org/dev/developers/testing/chromium-build-infrastructure/buildbot-annotations
  Args:
    format: gyp format to test.
  Returns:
    0 for sucesss, 1 for failure.
  """
  if not format:
    format = title

  # Install ninja if needed.
  # NOTE: as ninja gets installed each time, regressions to ninja can come
  # either from changes to ninja itself, or changes to gyp.
  if format == 'ninja':
    try:
      InstallNinja()
    except Exception, e:
      print '@@@STEP_FAILURE@@@'
      print str(e)
      return 1

  print '@@@BUILD_STEP ' + title + '@@@'
  sys.stdout.flush()
  env = os.environ.copy()
  # TODO(bradnelson): remove this when this issue is resolved:
  #     http://code.google.com/p/chromium/issues/detail?id=108251
  if format == 'ninja':
    env['NOGOLD'] = '1'
  if msvs_version:
    env['GYP_MSVS_VERSION'] = msvs_version
  retcode = subprocess.call(' '.join(
      [sys.executable, 'trunk/gyptest.py',
       '--all',
       '--passed',
       '--format', format,
       '--chdir', 'trunk',
       '--path', '../scons']),
      cwd=ROOT_DIR, env=env, shell=True)
  if retcode:
    # Emit failure tag, and keep going.
    print '@@@STEP_FAILURE@@@'
    return 1
  return 0


def GypBuild():
  # Dump out/ directory.
  print '@@@BUILD_STEP cleanup@@@'
  print 'Removing %s...' % OUT_DIR
  shutil.rmtree(OUT_DIR, ignore_errors=True)
  print 'Removing %s...' % NINJA_WORK_DIR
  shutil.rmtree(NINJA_WORK_DIR, ignore_errors=True)
  print 'Removing %s...' % NINJA_PATH
  shutil.rmtree(NINJA_PATH, ignore_errors=True)
  print 'Done.'

  retcode = 0
  if sys.platform.startswith('linux'):
    retcode += GypTestFormat('ninja')
    retcode += GypTestFormat('scons')
    retcode += GypTestFormat('make')
  elif sys.platform == 'darwin':
    retcode += GypTestFormat('ninja')
    retcode += GypTestFormat('xcode')
    retcode += GypTestFormat('make')
  elif sys.platform == 'win32':
    retcode += GypTestFormat('msvs-2008', format='msvs', msvs_version='2008')
    if os.environ['BUILDBOT_BUILDERNAME'] == 'gyp-win64':
      retcode += GypTestFormat('msvs-2010', format='msvs', msvs_version='2010')
  else:
    raise Exception('Unknown platform')
  if retcode:
    # TODO(bradnelson): once the annotator supports a postscript (section for
    #     after the build proper that could be used for cumulative failures),
    #     use that instead of this. This isolates the final return value so
    #     that it isn't misattributed to the last stage.
    print '@@@BUILD_STEP failures@@@'
    sys.exit(retcode)


if __name__ == '__main__':
  GypBuild()
