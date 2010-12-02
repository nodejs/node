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

"""Unit tests for JavaScriptStateTracker."""



import unittest as googletest
from closure_linter import javascriptstatetracker

class JavaScriptStateTrackerTest(googletest.TestCase):

  __test_cases = {
    'package.CONSTANT' : 'package',
    'package.methodName' : 'package',
    'package.subpackage.methodName' : 'package.subpackage',
    'package.ClassName.something' : 'package.ClassName',
    'package.ClassName.Enum.VALUE.methodName' : 'package.ClassName.Enum',
    'package.ClassName.CONSTANT' : 'package.ClassName',
    'package.ClassName.inherits' : 'package.ClassName',
    'package.ClassName.apply' : 'package.ClassName',
    'package.ClassName.methodName.apply' : 'package.ClassName',
    'package.ClassName.methodName.call' : 'package.ClassName',
    'package.ClassName.prototype.methodName' : 'package.ClassName',
    'package.ClassName.privateMethod_' : None,
    'package.ClassName.prototype.methodName.apply' : 'package.ClassName'
  }

  def testGetClosurizedNamespace(self):
    stateTracker = javascriptstatetracker.JavaScriptStateTracker(['package'])
    for identifier, expected_namespace in self.__test_cases.items():
      actual_namespace = stateTracker.GetClosurizedNamespace(identifier)
      self.assertEqual(expected_namespace, actual_namespace,
          'expected namespace "' + str(expected_namespace) +
              '" for identifier "' + str(identifier) + '" but was "' +
              str(actual_namespace) + '"')

if __name__ == '__main__':
  googletest.main()

