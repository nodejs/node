#!/usr/bin/env python
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

import sys
import time

from closure_linter import checker
from closure_linter import errors
from closure_linter.common import errorprinter
from closure_linter.common import simplefileflags as fileflags
import gflags as flags


FLAGS = flags.FLAGS
flags.DEFINE_boolean('unix_mode', False,
                     'Whether to emit warnings in standard unix format.')
flags.DEFINE_boolean('beep', True, 'Whether to beep when errors are found.')
flags.DEFINE_boolean('time', False, 'Whether to emit timing statistics.')
flags.DEFINE_boolean('check_html', False,
                     'Whether to check javascript in html files.')
flags.DEFINE_boolean('summary', False,
                     'Whether to show an error count summary.')

GJSLINT_ONLY_FLAGS = ['--unix_mode', '--beep', '--nobeep', '--time',
                      '--check_html', '--summary']


def FormatTime(t):
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
  if FLAGS.check_html:
    suffixes += ['.html', '.htm']
  files = fileflags.GetFileList(argv, 'JavaScript', suffixes)

  error_handler = None
  if FLAGS.unix_mode:
    error_handler = errorprinter.ErrorPrinter(errors.NEW_ERRORS)
    error_handler.SetFormat(errorprinter.UNIX_FORMAT)

  runner = checker.GJsLintRunner()
  result = runner.Run(files, error_handler)
  result.PrintSummary()

  exit_code = 0
  if result.HasOldErrors():
    exit_code += 1
  if result.HasNewErrors():
    exit_code += 2

  if exit_code:
    if FLAGS.summary:
      result.PrintFileSummary()

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

fixjsstyle %s
""" % ' '.join(fix_args)

  if FLAGS.time:
    print 'Done in %s.' % FormatTime(time.time() - start_time)

  sys.exit(exit_code)


if __name__ == '__main__':
  main()
