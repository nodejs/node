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

"""Automatically fix simple style guide violations."""

__author__ = 'robbyw@google.com (Robert Walker)'

import StringIO
import sys

import gflags as flags

from closure_linter import error_fixer
from closure_linter import runner
from closure_linter.common import simplefileflags as fileflags

FLAGS = flags.FLAGS
flags.DEFINE_list('additional_extensions', None, 'List of additional file '
                  'extensions (not js) that should be treated as '
                  'JavaScript files.')
flags.DEFINE_boolean('dry_run', False, 'Do not modify the file, only print it.')


def main(argv=None):
  """Main function.

  Args:
    argv: Sequence of command line arguments.
  """
  if argv is None:
    argv = flags.FLAGS(sys.argv)

  suffixes = ['.js']
  if FLAGS.additional_extensions:
    suffixes += ['.%s' % ext for ext in FLAGS.additional_extensions]

  files = fileflags.GetFileList(argv, 'JavaScript', suffixes)

  output_buffer = None
  if FLAGS.dry_run:
    output_buffer = StringIO.StringIO()

  fixer = error_fixer.ErrorFixer(output_buffer)

  # Check the list of files.
  for filename in files:
    runner.Run(filename, fixer)
    if FLAGS.dry_run:
      print output_buffer.getvalue()


if __name__ == '__main__':
  main()
