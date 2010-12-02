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


def ShouldReportError(error):
  """Whether the given error should be reported.
  
  Returns:
    True for all errors except missing documentation errors.  For these,
    it returns the value of the jsdoc flag.
  """
  return FLAGS.jsdoc or error not in (
      errors.MISSING_PARAMETER_DOCUMENTATION,
      errors.MISSING_RETURN_DOCUMENTATION,
      errors.MISSING_MEMBER_DOCUMENTATION,
      errors.MISSING_PRIVATE,
      errors.MISSING_JSDOC_TAG_THIS)
