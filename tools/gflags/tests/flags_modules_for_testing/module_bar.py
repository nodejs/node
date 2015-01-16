#!/usr/bin/env python

# Copyright (c) 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Auxiliary module for testing gflags.py.

The purpose of this module is to define a few flags.  We want to make
sure the unit tests for gflags.py involve more than one module.
"""

__author__ = 'salcianu@google.com (Alex Salcianu)'

__pychecker__ = 'no-local'  # for unittest

import gflags

FLAGS = gflags.FLAGS


def DefineFlags(flag_values=FLAGS):
  """Defines some flags.

  Args:
    flag_values: The FlagValues object we want to register the flags
      with.
  """
  # The 'tmod_bar_' prefix (short for 'test_module_bar') ensures there
  # is no name clash with the existing flags.
  gflags.DEFINE_boolean('tmod_bar_x', True, 'Boolean flag.',
                       flag_values=flag_values)
  gflags.DEFINE_string('tmod_bar_y', 'default', 'String flag.',
                      flag_values=flag_values)
  gflags.DEFINE_boolean('tmod_bar_z', False,
                       'Another boolean flag from module bar.',
                       flag_values=flag_values)
  gflags.DEFINE_integer('tmod_bar_t', 4, 'Sample int flag.',
                       flag_values=flag_values)
  gflags.DEFINE_integer('tmod_bar_u', 5, 'Sample int flag.',
                       flag_values=flag_values)
  gflags.DEFINE_integer('tmod_bar_v', 6, 'Sample int flag.',
                       flag_values=flag_values)


def RemoveOneFlag(flag_name, flag_values=FLAGS):
  """Removes the definition of one flag from gflags.FLAGS.

  Note: if the flag is not defined in gflags.FLAGS, this function does
  not do anything (in particular, it does not raise any exception).

  Motivation: We use this function for cleanup *after* a test: if
  there was a failure during a test and not all flags were declared,
  we do not want the cleanup code to crash.

  Args:
    flag_name: A string, the name of the flag to delete.
    flag_values: The FlagValues object we remove the flag from.
  """
  if flag_name in flag_values.FlagDict():
    flag_values.__delattr__(flag_name)


def NamesOfDefinedFlags():
  """Returns: List of names of the flags declared in this module."""
  return ['tmod_bar_x',
          'tmod_bar_y',
          'tmod_bar_z',
          'tmod_bar_t',
          'tmod_bar_u',
          'tmod_bar_v']


def RemoveFlags(flag_values=FLAGS):
  """Deletes the flag definitions done by the above DefineFlags().

  Args:
    flag_values: The FlagValues object we remove the flags from.
  """
  for flag_name in NamesOfDefinedFlags():
    RemoveOneFlag(flag_name, flag_values=flag_values)


def GetModuleName():
  """Uses gflags._GetCallingModule() to return the name of this module.

  For checking that _GetCallingModule works as expected.

  Returns:
    A string, the name of this module.
  """
  # Calling the protected _GetCallingModule generates a lint warning,
  # but we do not have any other alternative to test that function.
  return gflags._GetCallingModule()


def ExecuteCode(code, global_dict):
  """Executes some code in a given global environment.

  For testing of _GetCallingModule.

  Args:
    code: A string, the code to be executed.
    global_dict: A dictionary, the global environment that code should
      be executed in.
  """
  # Indeed, using exec generates a lint warning.  But some user code
  # actually uses exec, and we have to test for it ...
  exec code in global_dict
