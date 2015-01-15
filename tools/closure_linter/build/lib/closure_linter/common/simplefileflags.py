#!/usr/bin/env python
#
# Copyright 2008 The Closure Linter Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Determines the list of files to be checked from command line arguments."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

import glob
import os
import re

import gflags as flags


FLAGS = flags.FLAGS

flags.DEFINE_multistring(
    'recurse',
    None,
    'Recurse in to the subdirectories of the given path',
    short_name='r')
flags.DEFINE_list(
    'exclude_directories',
    ('_demos'),
    'Exclude the specified directories (only applicable along with -r or '
    '--presubmit)',
    short_name='e')
flags.DEFINE_list(
    'exclude_files',
    ('deps.js'),
    'Exclude the specified files',
    short_name='x')


def MatchesSuffixes(filename, suffixes):
  """Returns whether the given filename matches one of the given suffixes.

  Args:
    filename: Filename to check.
    suffixes: Sequence of suffixes to check.

  Returns:
    Whether the given filename matches one of the given suffixes.
  """
  suffix = filename[filename.rfind('.'):]
  return suffix in suffixes


def _GetUserSpecifiedFiles(argv, suffixes):
  """Returns files to be linted, specified directly on the command line.

  Can handle the '*' wildcard in filenames, but no other wildcards.

  Args:
    argv: Sequence of command line arguments. The second and following arguments
      are assumed to be files that should be linted.
    suffixes: Expected suffixes for the file type being checked.

  Returns:
    A sequence of files to be linted.
  """
  files = argv[1:] or []
  all_files = []
  lint_files = []

   # Perform any necessary globs.
  for f in files:
    if f.find('*') != -1:
      for result in glob.glob(f):
        all_files.append(result)
    else:
      all_files.append(f)

  for f in all_files:
    if MatchesSuffixes(f, suffixes):
      lint_files.append(f)
  return lint_files


def _GetRecursiveFiles(suffixes):
  """Returns files to be checked specified by the --recurse flag.

  Args:
    suffixes: Expected suffixes for the file type being checked.

  Returns:
    A list of files to be checked.
  """
  lint_files = []
  # Perform any request recursion
  if FLAGS.recurse:
    for start in FLAGS.recurse:
      for root, subdirs, files in os.walk(start):
        for f in files:
          if MatchesSuffixes(f, suffixes):
            lint_files.append(os.path.join(root, f))
  return lint_files


def GetAllSpecifiedFiles(argv, suffixes):
  """Returns all files specified by the user on the commandline.

  Args:
    argv: Sequence of command line arguments. The second and following arguments
      are assumed to be files that should be linted.
    suffixes: Expected suffixes for the file type

  Returns:
    A list of all files specified directly or indirectly (via flags) on the
    command line by the user.
  """
  files = _GetUserSpecifiedFiles(argv, suffixes)

  if FLAGS.recurse:
    files += _GetRecursiveFiles(suffixes)

  return FilterFiles(files)


def FilterFiles(files):
  """Filters the list of files to be linted be removing any excluded files.

  Filters out files excluded using --exclude_files and  --exclude_directories.

  Args:
    files: Sequence of files that needs filtering.

  Returns:
    Filtered list of files to be linted.
  """
  num_files = len(files)

  ignore_dirs_regexs = []
  for ignore in FLAGS.exclude_directories:
    ignore_dirs_regexs.append(re.compile(r'(^|[\\/])%s[\\/]' % ignore))

  result_files = []
  for f in files:
    add_file = True
    for exclude in FLAGS.exclude_files:
      if f.endswith('/' + exclude) or f == exclude:
        add_file = False
        break
    for ignore in ignore_dirs_regexs:
      if ignore.search(f):
        # Break out of ignore loop so we don't add to
        # filtered files.
        add_file = False
        break
    if add_file:
      # Convert everything to absolute paths so we can easily remove duplicates
      # using a set.
      result_files.append(os.path.abspath(f))

  skipped = num_files - len(result_files)
  if skipped:
    print 'Skipping %d file(s).' % skipped

  return set(result_files)


def GetFileList(argv, file_type, suffixes):
  """Parse the flags and return the list of files to check.

  Args:
    argv: Sequence of command line arguments.
    suffixes: Sequence of acceptable suffixes for the file type.

  Returns:
    The list of files to check.
  """
  return sorted(GetAllSpecifiedFiles(argv, suffixes))


def IsEmptyArgumentList(argv):
  return not (len(argv[1:]) or FLAGS.recurse)
