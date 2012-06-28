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

import sys

import gflags as flags
from closure_linter import checker
from closure_linter import error_fixer
from closure_linter.common import simplefileflags as fileflags


def main(argv = None):
  """Main function.

  Args:
    argv: Sequence of command line arguments.
  """
  if argv is None:
    argv = flags.FLAGS(sys.argv)

  files = fileflags.GetFileList(argv, 'JavaScript', ['.js'])

  style_checker = checker.JavaScriptStyleChecker(error_fixer.ErrorFixer())

  # Check the list of files.
  for filename in files:
    style_checker.Check(filename)

if __name__ == '__main__':
  main()
