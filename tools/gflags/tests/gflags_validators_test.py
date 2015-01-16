#!/usr/bin/env python

# Copyright (c) 2010, Google Inc.
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

"""Testing that flags validators framework does work.

This file tests that each flag validator called when it should be, and that
failed validator will throw an exception, etc.
"""

__author__ = 'olexiy@google.com (Olexiy Oryeshko)'

import gflags_googletest as googletest
import gflags
import gflags_validators


class SimpleValidatorTest(googletest.TestCase):
  """Testing gflags.RegisterValidator() method."""

  def setUp(self):
    super(SimpleValidatorTest, self).setUp()
    self.flag_values = gflags.FlagValues()
    self.call_args = []

  def testSuccess(self):
    def Checker(x):
      self.call_args.append(x)
      return True
    gflags.DEFINE_integer('test_flag', None, 'Usual integer flag',
                         flag_values=self.flag_values)
    gflags.RegisterValidator('test_flag',
                            Checker,
                            message='Errors happen',
                            flag_values=self.flag_values)

    argv = ('./program')
    self.flag_values(argv)
    self.assertEquals(None, self.flag_values.test_flag)
    self.flag_values.test_flag = 2
    self.assertEquals(2, self.flag_values.test_flag)
    self.assertEquals([None, 2], self.call_args)

  def testDefaultValueNotUsedSuccess(self):
    def Checker(x):
      self.call_args.append(x)
      return True
    gflags.DEFINE_integer('test_flag', None, 'Usual integer flag',
                         flag_values=self.flag_values)
    gflags.RegisterValidator('test_flag',
                            Checker,
                            message='Errors happen',
                            flag_values=self.flag_values)

    argv = ('./program', '--test_flag=1')
    self.flag_values(argv)
    self.assertEquals(1, self.flag_values.test_flag)
    self.assertEquals([1], self.call_args)

  def testValidatorNotCalledWhenOtherFlagIsChanged(self):
    def Checker(x):
      self.call_args.append(x)
      return True
    gflags.DEFINE_integer('test_flag', 1, 'Usual integer flag',
                         flag_values=self.flag_values)
    gflags.DEFINE_integer('other_flag', 2, 'Other integer flag',
                         flag_values=self.flag_values)
    gflags.RegisterValidator('test_flag',
                            Checker,
                            message='Errors happen',
                            flag_values=self.flag_values)

    argv = ('./program')
    self.flag_values(argv)
    self.assertEquals(1, self.flag_values.test_flag)
    self.flag_values.other_flag = 3
    self.assertEquals([1], self.call_args)

  def testExceptionRaisedIfCheckerFails(self):
    def Checker(x):
      self.call_args.append(x)
      return x == 1
    gflags.DEFINE_integer('test_flag', None, 'Usual integer flag',
                         flag_values=self.flag_values)
    gflags.RegisterValidator('test_flag',
                            Checker,
                            message='Errors happen',
                            flag_values=self.flag_values)

    argv = ('./program', '--test_flag=1')
    self.flag_values(argv)
    try:
      self.flag_values.test_flag = 2
      raise AssertionError('gflags.IllegalFlagValue expected')
    except gflags.IllegalFlagValue, e:
      self.assertEquals('flag --test_flag=2: Errors happen', str(e))
    self.assertEquals([1, 2], self.call_args)

  def testExceptionRaisedIfCheckerRaisesException(self):
    def Checker(x):
      self.call_args.append(x)
      if x == 1:
        return True
      raise gflags_validators.Error('Specific message')
    gflags.DEFINE_integer('test_flag', None, 'Usual integer flag',
                         flag_values=self.flag_values)
    gflags.RegisterValidator('test_flag',
                            Checker,
                            message='Errors happen',
                            flag_values=self.flag_values)

    argv = ('./program', '--test_flag=1')
    self.flag_values(argv)
    try:
      self.flag_values.test_flag = 2
      raise AssertionError('gflags.IllegalFlagValue expected')
    except gflags.IllegalFlagValue, e:
      self.assertEquals('flag --test_flag=2: Specific message', str(e))
    self.assertEquals([1, 2], self.call_args)

  def testErrorMessageWhenCheckerReturnsFalseOnStart(self):
    def Checker(x):
      self.call_args.append(x)
      return False
    gflags.DEFINE_integer('test_flag', None, 'Usual integer flag',
                         flag_values=self.flag_values)
    gflags.RegisterValidator('test_flag',
                            Checker,
                            message='Errors happen',
                            flag_values=self.flag_values)

    argv = ('./program', '--test_flag=1')
    try:
      self.flag_values(argv)
      raise AssertionError('gflags.IllegalFlagValue expected')
    except gflags.IllegalFlagValue, e:
      self.assertEquals('flag --test_flag=1: Errors happen', str(e))
    self.assertEquals([1], self.call_args)

  def testErrorMessageWhenCheckerRaisesExceptionOnStart(self):
    def Checker(x):
      self.call_args.append(x)
      raise gflags_validators.Error('Specific message')
    gflags.DEFINE_integer('test_flag', None, 'Usual integer flag',
                         flag_values=self.flag_values)
    gflags.RegisterValidator('test_flag',
                            Checker,
                            message='Errors happen',
                            flag_values=self.flag_values)

    argv = ('./program', '--test_flag=1')
    try:
      self.flag_values(argv)
      raise AssertionError('IllegalFlagValue expected')
    except gflags.IllegalFlagValue, e:
      self.assertEquals('flag --test_flag=1: Specific message', str(e))
    self.assertEquals([1], self.call_args)

  def testValidatorsCheckedInOrder(self):

    def Required(x):
      self.calls.append('Required')
      return x is not None

    def Even(x):
      self.calls.append('Even')
      return x % 2 == 0

    self.calls = []
    self._DefineFlagAndValidators(Required, Even)
    self.assertEquals(['Required', 'Even'], self.calls)

    self.calls = []
    self._DefineFlagAndValidators(Even, Required)
    self.assertEquals(['Even', 'Required'], self.calls)

  def _DefineFlagAndValidators(self, first_validator, second_validator):
    local_flags = gflags.FlagValues()
    gflags.DEFINE_integer('test_flag', 2, 'test flag', flag_values=local_flags)
    gflags.RegisterValidator('test_flag',
                            first_validator,
                            message='',
                            flag_values=local_flags)
    gflags.RegisterValidator('test_flag',
                            second_validator,
                            message='',
                            flag_values=local_flags)
    argv = ('./program')
    local_flags(argv)


if __name__ == '__main__':
  googletest.main()
