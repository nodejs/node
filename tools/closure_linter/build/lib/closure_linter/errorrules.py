#!/usr/bin/env python
#
# Copyright 2010 The Closure Linter Authors. All Rights Reserved.
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

"""Linter error rules class for Closure Linter."""

__author__ = 'robbyw@google.com (Robert Walker)'

import gflags as flags
from closure_linter import errors


FLAGS = flags.FLAGS
flags.DEFINE_boolean('jsdoc', True,
                     'Whether to report errors for missing JsDoc.')
flags.DEFINE_list('disable', None,
                  'Disable specific error. Usage Ex.: gjslint --disable 1,'
                  '0011 foo.js.')
flags.DEFINE_integer('max_line_length', 80, 'Maximum line length allowed '
                     'without warning.', lower_bound=1)

disabled_error_nums = None


def GetMaxLineLength():
  """Returns allowed maximum length of line.

  Returns:
    Length of line allowed without any warning.
  """
  return FLAGS.max_line_length


def ShouldReportError(error):
  """Whether the given error should be reported.

  Returns:
    True for all errors except missing documentation errors and disabled
    errors.  For missing documentation, it returns the value of the
    jsdoc flag.
  """
  global disabled_error_nums
  if disabled_error_nums is None:
    disabled_error_nums = []
    if FLAGS.disable:
      for error_str in FLAGS.disable:
        error_num = 0
        try:
          error_num = int(error_str)
        except ValueError:
          pass
        disabled_error_nums.append(error_num)

  return ((FLAGS.jsdoc or error not in (
      errors.MISSING_PARAMETER_DOCUMENTATION,
      errors.MISSING_RETURN_DOCUMENTATION,
      errors.MISSING_MEMBER_DOCUMENTATION,
      errors.MISSING_PRIVATE,
      errors.MISSING_JSDOC_TAG_THIS)) and
          (not FLAGS.disable or error not in disabled_error_nums))
