#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
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
ANDROID_DIR = os.path.join(ROOT_DIR, 'android')
OUT_DIR = os.path.join(TRUNK_DIR, 'out')


def CallSubProcess(*args, **kwargs):
  """Wrapper around subprocess.call which treats errors as build exceptions."""
  retcode = subprocess.call(*args, **kwargs)
  if retcode != 0:
    print '@@@STEP_EXCEPTION@@@'
    sys.exit(1)


def PrepareAndroidTree():
  """Prepare an Android tree to run 'android' format tests."""
  if os.environ['BUILDBOT_CLOBBER'] == '1':
    print '@@@BUILD_STEP Clobber Android checkout@@@'
    shutil.rmtree(ANDROID_DIR)

  # The release of Android we use is static, so there's no need to do anything
  # if the directory already exists.
  if os.path.isdir(ANDROID_DIR):
    return

  print '@@@BUILD_STEP Initialize Android checkout@@@'
  os.mkdir(ANDROID_DIR)
  CallSubProcess(['git', 'config', '--global', 'user.name', 'trybot'])
  CallSubProcess(['git', 'config', '--global',
                  'user.email', 'chrome-bot@google.com'])
  CallSubProcess(['git', 'config', '--global', 'color.ui', 'false'])
  CallSubProcess(
      ['repo', 'init',
       '-u', 'https://android.googlesource.com/platform/manifest',
       '-b', 'android-4.2.1_r1',
       '-g', 'all,-notdefault,-device,-darwin,-mips,-x86'],
      cwd=ANDROID_DIR)

  print '@@@BUILD_STEP Sync Android@@@'
  CallSubProcess(['repo', 'sync', '-j4'], cwd=ANDROID_DIR)

  print '@@@BUILD_STEP Build Android@@@'
  CallSubProcess(
      ['/bin/bash',
       '-c', 'source build/envsetup.sh && lunch full-eng && make -j4'],
      cwd=ANDROID_DIR)


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

  print '@@@BUILD_STEP ' + title + '@@@'
  sys.stdout.flush()
  env = os.environ.copy()
  if msvs_version:
    env['GYP_MSVS_VERSION'] = msvs_version
  command = ' '.join(
      [sys.executable, 'trunk/gyptest.py',
       '--all',
       '--passed',
       '--format', format,
       '--chdir', 'trunk'])
  if format == 'android':
    # gyptest needs the environment setup from envsetup/lunch in order to build
    # using the 'android' backend, so this is done in a single shell.
    retcode = subprocess.call(
        ['/bin/bash',
         '-c', 'source build/envsetup.sh && lunch full-eng && cd %s && %s'
         % (ROOT_DIR, command)],
        cwd=ANDROID_DIR, env=env)
  else:
    retcode = subprocess.call(command, cwd=ROOT_DIR, env=env, shell=True)
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
  print 'Done.'

  retcode = 0
  # The Android gyp bot runs on linux so this must be tested first.
  if os.environ['BUILDBOT_BUILDERNAME'] == 'gyp-android':
    PrepareAndroidTree()
    retcode += GypTestFormat('android')
  elif sys.platform.startswith('linux'):
    retcode += GypTestFormat('ninja')
    retcode += GypTestFormat('make')
  elif sys.platform == 'darwin':
    retcode += GypTestFormat('ninja')
    retcode += GypTestFormat('xcode')
    retcode += GypTestFormat('make')
  elif sys.platform == 'win32':
    retcode += GypTestFormat('ninja')
    if os.environ['BUILDBOT_BUILDERNAME'] == 'gyp-win64':
      retcode += GypTestFormat('msvs-2010', format='msvs', msvs_version='2010')
      retcode += GypTestFormat('msvs-2012', format='msvs', msvs_version='2012')
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
