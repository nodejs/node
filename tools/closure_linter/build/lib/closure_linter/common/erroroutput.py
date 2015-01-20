#!/usr/bin/env python
#
# Copyright 2012 The Closure Linter Authors. All Rights Reserved.
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

"""Utility functions to format errors."""


__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)',
              'nnaze@google.com (Nathan Naze)')


def GetUnixErrorOutput(filename, error, new_error=False):
  """Get a output line for an error in UNIX format."""

  line = ''

  if error.token:
    line = '%d' % error.token.line_number

  error_code = '%04d' % error.code
  if new_error:
    error_code = 'New Error ' + error_code
  return '%s:%s:(%s) %s' % (filename, line, error_code, error.message)


def GetErrorOutput(error, new_error=False):
  """Get a output line for an error in regular format."""

  line = ''
  if error.token:
    line = 'Line %d, ' % error.token.line_number

  code = 'E:%04d' % error.code

  error_message = error.message
  if new_error:
    error_message = 'New Error ' + error_message

  return '%s%s: %s' % (line, code, error.message)
