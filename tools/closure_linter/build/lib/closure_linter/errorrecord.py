#!/usr/bin/env python
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


"""A simple, pickle-serializable class to represent a lint error."""

__author__ = 'nnaze@google.com (Nathan Naze)'

import gflags as flags

from closure_linter import errors
from closure_linter.common import erroroutput

FLAGS = flags.FLAGS


class ErrorRecord(object):
  """Record-keeping struct that can be serialized back from a process.

  Attributes:
    path: Path to the file.
    error_string: Error string for the user.
    new_error: Whether this is a "new error" (see errors.NEW_ERRORS).
  """

  def __init__(self, path, error_string, new_error):
    self.path = path
    self.error_string = error_string
    self.new_error = new_error


def MakeErrorRecord(path, error):
  """Make an error record with correctly formatted error string.

  Errors are not able to be serialized (pickled) over processes because of
  their pointers to the complex token/context graph.  We use an intermediary
  serializable class to pass back just the relevant information.

  Args:
    path: Path of file the error was found in.
    error: An error.Error instance.

  Returns:
    _ErrorRecord instance.
  """
  new_error = error.code in errors.NEW_ERRORS

  if FLAGS.unix_mode:
    error_string = erroroutput.GetUnixErrorOutput(
        path, error, new_error=new_error)
  else:
    error_string = erroroutput.GetErrorOutput(error, new_error=new_error)

  return ErrorRecord(path, error_string, new_error)
