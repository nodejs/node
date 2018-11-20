# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Sets up various automatic gyp environment variables. These are used by
gyp_v8 and landmines.py which run at different stages of runhooks. To
make sure settings are consistent between them, all setup should happen here.
"""

import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
V8_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir))


def apply_gyp_environment(file_path=None):
  """
  Reads in a *.gyp_env file and applies the valid keys to os.environ.
  """
  if not file_path or not os.path.exists(file_path):
    return
  file_contents = open(file_path).read()
  try:
    file_data = eval(file_contents, {'__builtins__': None}, None)
  except SyntaxError, e:
    e.filename = os.path.abspath(file_path)
    raise
  supported_vars = ( 'V8_GYP_FILE',
                     'V8_GYP_SYNTAX_CHECK',
                     'GYP_DEFINES',
                     'GYP_GENERATORS',
                     'GYP_GENERATOR_FLAGS',
                     'GYP_GENERATOR_OUTPUT', )
  for var in supported_vars:
    val = file_data.get(var)
    if val:
      if var in os.environ:
        print 'INFO: Environment value for "%s" overrides value in %s.' % (
            var, os.path.abspath(file_path)
        )
      else:
        os.environ[var] = val


def set_environment():
  """Sets defaults for GYP_* variables."""

  if 'SKIP_V8_GYP_ENV' not in os.environ:
    # Update the environment based on v8.gyp_env
    gyp_env_path = os.path.join(os.path.dirname(V8_ROOT), 'v8.gyp_env')
    apply_gyp_environment(gyp_env_path)

    if not os.environ.get('GYP_GENERATORS'):
      # Default to ninja on all platforms.
      os.environ['GYP_GENERATORS'] = 'ninja'
