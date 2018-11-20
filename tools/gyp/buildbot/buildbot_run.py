#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Argument-less script to select what to run on the buildbots."""

import os
import shutil
import subprocess
import sys


BUILDBOT_DIR = os.path.dirname(os.path.abspath(__file__))
TRUNK_DIR = os.path.dirname(BUILDBOT_DIR)
ROOT_DIR = os.path.dirname(TRUNK_DIR)
CMAKE_DIR = os.path.join(ROOT_DIR, 'cmake')
CMAKE_BIN_DIR = os.path.join(CMAKE_DIR, 'bin')
OUT_DIR = os.path.join(TRUNK_DIR, 'out')


def CallSubProcess(*args, **kwargs):
  """Wrapper around subprocess.call which treats errors as build exceptions."""
  with open(os.devnull) as devnull_fd:
    retcode = subprocess.call(stdin=devnull_fd, *args, **kwargs)
  if retcode != 0:
    print '@@@STEP_EXCEPTION@@@'
    sys.exit(1)


def PrepareCmake():
  """Build CMake 2.8.8 since the version in Precise is 2.8.7."""
  if os.environ['BUILDBOT_CLOBBER'] == '1':
    print '@@@BUILD_STEP Clobber CMake checkout@@@'
    shutil.rmtree(CMAKE_DIR)

  # We always build CMake 2.8.8, so no need to do anything
  # if the directory already exists.
  if os.path.isdir(CMAKE_DIR):
    return

  print '@@@BUILD_STEP Initialize CMake checkout@@@'
  os.mkdir(CMAKE_DIR)

  print '@@@BUILD_STEP Sync CMake@@@'
  CallSubProcess(
      ['git', 'clone',
       '--depth', '1',
       '--single-branch',
       '--branch', 'v2.8.8',
       '--',
       'git://cmake.org/cmake.git',
       CMAKE_DIR],
      cwd=CMAKE_DIR)

  print '@@@BUILD_STEP Build CMake@@@'
  CallSubProcess(
      ['/bin/bash', 'bootstrap', '--prefix=%s' % CMAKE_DIR],
      cwd=CMAKE_DIR)

  CallSubProcess( ['make', 'cmake'], cwd=CMAKE_DIR)


def GypTestFormat(title, format=None, msvs_version=None, tests=[]):
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
      [sys.executable, 'gyp/gyptest.py',
       '--all',
       '--passed',
       '--format', format,
       '--path', CMAKE_BIN_DIR,
       '--chdir', 'gyp'] + tests)
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
  if sys.platform.startswith('linux'):
    retcode += GypTestFormat('ninja')
    retcode += GypTestFormat('make')
    PrepareCmake()
    retcode += GypTestFormat('cmake')
  elif sys.platform == 'darwin':
    retcode += GypTestFormat('ninja')
    retcode += GypTestFormat('xcode')
    retcode += GypTestFormat('make')
  elif sys.platform == 'win32':
    retcode += GypTestFormat('ninja')
    if os.environ['BUILDBOT_BUILDERNAME'] == 'gyp-win64':
      retcode += GypTestFormat('msvs-ninja-2013', format='msvs-ninja',
                               msvs_version='2013',
                               tests=[
                                  r'test\generator-output\gyptest-actions.py',
                                  r'test\generator-output\gyptest-relocate.py',
                                  r'test\generator-output\gyptest-rules.py'])
      retcode += GypTestFormat('msvs-2013', format='msvs', msvs_version='2013')
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
