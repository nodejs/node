#!/usr/bin/env python
# python2.6 for command-line runs using p4lib.  pylint: disable-msg=C6301
#
# Copyright 2007 The Closure Linter Authors. All Rights Reserved.
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

"""Checks JavaScript files for common style guide violations.

gjslint.py is designed to be used as a PRESUBMIT script to check for javascript
style guide violations.  As of now, it checks for the following violations:

  * Missing and extra spaces
  * Lines longer than 80 characters
  * Missing newline at end of file
  * Missing semicolon after function declaration
  * Valid JsDoc including parameter matching

Someday it will validate to the best of its ability against the entirety of the
JavaScript style guide.

This file is a front end that parses arguments and flags.  The core of the code
is in tokenizer.py and checker.py.
"""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

import functools
import itertools
import sys
import time

import gflags as flags

from closure_linter import checker
from closure_linter import errorrecord
from closure_linter.common import erroraccumulator
from closure_linter.common import simplefileflags as fileflags

# Attempt import of multiprocessing (should be available in Python 2.6 and up).
try:
  # pylint: disable-msg=C6204
  import multiprocessing
except ImportError:
  multiprocessing = None

FLAGS = flags.FLAGS
flags.DEFINE_boolean('unix_mode', False,
                     'Whether to emit warnings in standard unix format.')
flags.DEFINE_boolean('beep', True, 'Whether to beep when errors are found.')
flags.DEFINE_boolean('time', False, 'Whether to emit timing statistics.')
flags.DEFINE_boolean('check_html', False,
                     'Whether to check javascript in html files.')
flags.DEFINE_boolean('summary', False,
                     'Whether to show an error count summary.')
flags.DEFINE_list('additional_extensions', None, 'List of additional file '
                  'extensions (not js) that should be treated as '
                  'JavaScript files.')
flags.DEFINE_boolean('multiprocess', False,
                     'Whether to parallalize linting using the '
                     'multiprocessing module.  Disabled by default.')


GJSLINT_ONLY_FLAGS = ['--unix_mode', '--beep', '--nobeep', '--time',
                      '--check_html', '--summary']


def _MultiprocessCheckPaths(paths):
  """Run _CheckPath over mutltiple processes.

  Tokenization, passes, and checks are expensive operations.  Running in a
  single process, they can only run on one CPU/core.  Instead,
  shard out linting over all CPUs with multiprocessing to parallelize.

  Args:
    paths: paths to check.

  Yields:
    errorrecord.ErrorRecords for any found errors.
  """

  pool = multiprocessing.Pool()

  for results in pool.imap(_CheckPath, paths):
    for record in results:
      yield record

  pool.close()
  pool.join()


def _CheckPaths(paths):
  """Run _CheckPath on all paths in one thread.

  Args:
    paths: paths to check.

  Yields:
    errorrecord.ErrorRecords for any found errors.
  """

  for path in paths:
    results = _CheckPath(path)
    for record in results:
      yield record


def _CheckPath(path):
  """Check a path and return any errors.

  Args:
    path: paths to check.

  Returns:
    A list of errorrecord.ErrorRecords for any found errors.
  """

  error_accumulator = erroraccumulator.ErrorAccumulator()
  style_checker = checker.JavaScriptStyleChecker(error_accumulator)
  style_checker.Check(path)

  # Return any errors as error records.
  make_error_record = functools.partial(errorrecord.MakeErrorRecord, path)
  return map(make_error_record, error_accumulator.GetErrors())


def _GetFilePaths(argv):
  suffixes = ['.js']
  if FLAGS.additional_extensions:
    suffixes += ['.%s' % ext for ext in FLAGS.additional_extensions]
  if FLAGS.check_html:
    suffixes += ['.html', '.htm']
  return fileflags.GetFileList(argv, 'JavaScript', suffixes)


# Error printing functions


def _PrintFileSummary(paths, records):
  """Print a detailed summary of the number of errors in each file."""

  paths = list(paths)
  paths.sort()

  for path in paths:
    path_errors = [e for e in records if e.path == path]
    print '%s: %d' % (path, len(path_errors))


def _PrintFileSeparator(path):
  print '----- FILE  :  %s -----' % path


def _PrintSummary(paths, error_records):
  """Print a summary of the number of errors and files."""

  error_count = len(error_records)
  all_paths = set(paths)
  all_paths_count = len(all_paths)

  if error_count is 0:
    print '%d files checked, no errors found.' % all_paths_count

  new_error_count = len([e for e in error_records if e.new_error])

  error_paths = set([e.path for e in error_records])
  error_paths_count = len(error_paths)
  no_error_paths_count = all_paths_count - error_paths_count

  if error_count or new_error_count:
    print ('Found %d errors, including %d new errors, in %d files '
           '(%d files OK).' % (
               error_count,
               new_error_count,
               error_paths_count,
               no_error_paths_count))


def _PrintErrorRecords(error_records):
  """Print error records strings in the expected format."""

  current_path = None
  for record in error_records:

    if current_path != record.path:
      current_path = record.path
      if not FLAGS.unix_mode:
        _PrintFileSeparator(current_path)

    print record.error_string


def _FormatTime(t):
  """Formats a duration as a human-readable string.

  Args:
    t: A duration in seconds.

  Returns:
    A formatted duration string.
  """
  if t < 1:
    return '%dms' % round(t * 1000)
  else:
    return '%.2fs' % t


def main(argv = None):
  """Main function.

  Args:
    argv: Sequence of command line arguments.
  """
  if argv is None:
    argv = flags.FLAGS(sys.argv)

  if FLAGS.time:
    start_time = time.time()

  suffixes = ['.js']
  if FLAGS.additional_extensions:
    suffixes += ['.%s' % ext for ext in FLAGS.additional_extensions]
  if FLAGS.check_html:
    suffixes += ['.html', '.htm']
  paths = fileflags.GetFileList(argv, 'JavaScript', suffixes)

  if FLAGS.multiprocess:
    records_iter = _MultiprocessCheckPaths(paths)
  else:
    records_iter = _CheckPaths(paths)

  records_iter, records_iter_copy = itertools.tee(records_iter, 2)
  _PrintErrorRecords(records_iter_copy)

  error_records = list(records_iter)
  _PrintSummary(paths, error_records)

  exit_code = 0

  # If there are any errors
  if error_records:
    exit_code += 1

  # If there are any new errors
  if [r for r in error_records if r.new_error]:
    exit_code += 2

  if exit_code:
    if FLAGS.summary:
      _PrintFileSummary(paths, error_records)

    if FLAGS.beep:
      # Make a beep noise.
      sys.stdout.write(chr(7))

    # Write out instructions for using fixjsstyle script to fix some of the
    # reported errors.
    fix_args = []
    for flag in sys.argv[1:]:
      for f in GJSLINT_ONLY_FLAGS:
        if flag.startswith(f):
          break
      else:
        fix_args.append(flag)

    print """
Some of the errors reported by GJsLint may be auto-fixable using the script
fixjsstyle. Please double check any changes it makes and report any bugs. The
script can be run by executing:

fixjsstyle %s """ % ' '.join(fix_args)

  if FLAGS.time:
    print 'Done in %s.' % _FormatTime(time.time() - start_time)

  sys.exit(exit_code)


if __name__ == '__main__':
  main()
