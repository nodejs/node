#!/usr/bin/env python
#
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

The purpose of this module is to define a few flags, and declare some
other flags as being important.  We want to make sure the unit tests
for gflags.py involve more than one module.
"""

__author__ = 'salcianu@google.com (Alex Salcianu)'

__pychecker__ = 'no-local'  # for unittest

import gflags
from flags_modules_for_testing import module_bar

FLAGS = gflags.FLAGS


DECLARED_KEY_FLAGS = ['tmod_bar_x', 'tmod_bar_z', 'tmod_bar_t',
                      # Special (not user-defined) flag:
                      'flagfile']


def DefineFlags(flag_values=FLAGS):
  """Defines a few flags."""
  module_bar.DefineFlags(flag_values=flag_values)
  # The 'tmod_foo_' prefix (short for 'test_module_foo') ensures that we
  # have no name clash with existing flags.
  gflags.DEFINE_boolean('tmod_foo_bool', True, 'Boolean flag from module foo.',
                       flag_values=flag_values)
  gflags.DEFINE_string('tmod_foo_str', 'default', 'String flag.',
                      flag_values=flag_values)
  gflags.DEFINE_integer('tmod_foo_int', 3, 'Sample int flag.',
                       flag_values=flag_values)


def DeclareKeyFlags(flag_values=FLAGS):
  """Declares a few key flags."""
  for flag_name in DECLARED_KEY_FLAGS:
    gflags.DECLARE_key_flag(flag_name, flag_values=flag_values)


def DeclareExtraKeyFlags(flag_values=FLAGS):
  """Declares some extra key flags."""
  gflags.ADOPT_module_key_flags(module_bar, flag_values=flag_values)


def NamesOfDefinedFlags():
  """Returns: list of names of flags defined by this module."""
  return ['tmod_foo_bool', 'tmod_foo_str', 'tmod_foo_int']


def NamesOfDeclaredKeyFlags():
  """Returns: list of names of key flags for this module."""
  return NamesOfDefinedFlags() + DECLARED_KEY_FLAGS


def NamesOfDeclaredExtraKeyFlags():
  """Returns the list of names of additional key flags for this module.

  These are the flags that became key for this module only as a result
  of a call to DeclareExtraKeyFlags() above.  I.e., the flags declared
  by module_bar, that were not already declared as key for this
  module.

  Returns:
    The list of names of additional key flags for this module.
  """
  names_of_extra_key_flags = list(module_bar.NamesOfDefinedFlags())
  for flag_name in NamesOfDeclaredKeyFlags():
    while flag_name in names_of_extra_key_flags:
      names_of_extra_key_flags.remove(flag_name)
  return names_of_extra_key_flags


def RemoveFlags(flag_values=FLAGS):
  """Deletes the flag definitions done by the above DefineFlags()."""
  for flag_name in NamesOfDefinedFlags():
    module_bar.RemoveOneFlag(flag_name, flag_values=flag_values)
  module_bar.RemoveFlags(flag_values=flag_values)


def GetModuleName():
  """Uses gflags._GetCallingModule() to return the name of this module.

  For checking that _GetCallingModule works as expected.

  Returns:
    A string, the name of this module.
  """
  # Calling the protected _GetCallingModule generates a lint warning,
  # but we do not have any other alternative to test that function.
  return gflags._GetCallingModule()


def DuplicateFlags(flagnames=None):
  """Returns a new FlagValues object with the requested flagnames.

  Used to test DuplicateFlagError detection.

  Args:
    flagnames: str, A list of flag names to create.

  Returns:
    A FlagValues object with one boolean flag for each name in flagnames.
  """
  flag_values = gflags.FlagValues()
  for name in flagnames:
    gflags.DEFINE_boolean(name, False, 'Flag named %s' % (name,),
                         flag_values=flag_values)
  return flag_values
