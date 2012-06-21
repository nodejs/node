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

"""Unit tests for ClosurizedNamespacesInfo."""



import unittest as googletest
from closure_linter import closurizednamespacesinfo
from closure_linter import javascriptstatetracker
from closure_linter import javascripttokenizer
from closure_linter import javascripttokens
from closure_linter import tokenutil

# pylint: disable-msg=C6409
TokenType = javascripttokens.JavaScriptTokenType


class ClosurizedNamespacesInfoTest(googletest.TestCase):
  """Tests for ClosurizedNamespacesInfo."""

  _test_cases = {
      'goog.global.anything': None,
      'package.CONSTANT': 'package',
      'package.methodName': 'package',
      'package.subpackage.methodName': 'package.subpackage',
      'package.subpackage.methodName.apply': 'package.subpackage',
      'package.ClassName.something': 'package.ClassName',
      'package.ClassName.Enum.VALUE.methodName': 'package.ClassName',
      'package.ClassName.CONSTANT': 'package.ClassName',
      'package.namespace.CONSTANT.methodName': 'package.namespace',
      'package.ClassName.inherits': 'package.ClassName',
      'package.ClassName.apply': 'package.ClassName',
      'package.ClassName.methodName.apply': 'package.ClassName',
      'package.ClassName.methodName.call': 'package.ClassName',
      'package.ClassName.prototype.methodName': 'package.ClassName',
      'package.ClassName.privateMethod_': 'package.ClassName',
      'package.className.privateProperty_': 'package.className',
      'package.className.privateProperty_.methodName': 'package.className',
      'package.ClassName.PrivateEnum_': 'package.ClassName',
      'package.ClassName.prototype.methodName.apply': 'package.ClassName',
      'package.ClassName.property.subProperty': 'package.ClassName',
      'package.className.prototype.something.somethingElse': 'package.className'
  }

  _tokenizer = javascripttokenizer.JavaScriptTokenizer()

  def testGetClosurizedNamespace(self):
    """Tests that the correct namespace is returned for various identifiers."""
    namespaces_info = closurizednamespacesinfo.ClosurizedNamespacesInfo(
        closurized_namespaces=['package'], ignored_extra_namespaces=[])
    for identifier, expected_namespace in self._test_cases.items():
      actual_namespace = namespaces_info.GetClosurizedNamespace(identifier)
      self.assertEqual(
          expected_namespace,
          actual_namespace,
          'expected namespace "' + str(expected_namespace) +
          '" for identifier "' + str(identifier) + '" but was "' +
          str(actual_namespace) + '"')

  def testIgnoredExtraNamespaces(self):
    """Tests that ignored_extra_namespaces are ignored."""
    token = self._GetRequireTokens('package.Something')
    namespaces_info = closurizednamespacesinfo.ClosurizedNamespacesInfo(
        closurized_namespaces=['package'],
        ignored_extra_namespaces=['package.Something'])

    self.assertFalse(namespaces_info.IsExtraRequire(token),
                     'Should be valid since it is in ignored namespaces.')

    namespaces_info = closurizednamespacesinfo.ClosurizedNamespacesInfo(
        ['package'], [])

    self.assertTrue(namespaces_info.IsExtraRequire(token),
                    'Should be invalid since it is not in ignored namespaces.')

  def testIsExtraProvide_created(self):
    """Tests that provides for created namespaces are not extra."""
    input_lines = [
        'goog.provide(\'package.Foo\');',
        'package.Foo = function() {};'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertFalse(namespaces_info.IsExtraProvide(token),
                     'Should not be extra since it is created.')

  def testIsExtraProvide_createdIdentifier(self):
    """Tests that provides for created identifiers are not extra."""
    input_lines = [
        'goog.provide(\'package.Foo.methodName\');',
        'package.Foo.methodName = function() {};'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertFalse(namespaces_info.IsExtraProvide(token),
                     'Should not be extra since it is created.')

  def testIsExtraProvide_notCreated(self):
    """Tests that provides for non-created namespaces are extra."""
    input_lines = ['goog.provide(\'package.Foo\');']
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertTrue(namespaces_info.IsExtraProvide(token),
                    'Should be extra since it is not created.')

  def testIsExtraProvide_duplicate(self):
    """Tests that providing a namespace twice makes the second one extra."""
    input_lines = [
        'goog.provide(\'package.Foo\');',
        'goog.provide(\'package.Foo\');',
        'package.Foo = function() {};'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    # Advance to the second goog.provide token.
    token = tokenutil.Search(token.next, TokenType.IDENTIFIER)

    self.assertTrue(namespaces_info.IsExtraProvide(token),
                    'Should be extra since it is already provided.')

  def testIsExtraProvide_notClosurized(self):
    """Tests that provides of non-closurized namespaces are not extra."""
    input_lines = ['goog.provide(\'notclosurized.Foo\');']
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertFalse(namespaces_info.IsExtraProvide(token),
                     'Should not be extra since it is not closurized.')

  def testIsExtraRequire_used(self):
    """Tests that requires for used namespaces are not extra."""
    input_lines = [
        'goog.require(\'package.Foo\');',
        'var x = package.Foo.methodName();'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertFalse(namespaces_info.IsExtraRequire(token),
                     'Should not be extra since it is used.')

  def testIsExtraRequire_usedIdentifier(self):
    """Tests that requires for used methods on classes are extra."""
    input_lines = [
        'goog.require(\'package.Foo.methodName\');',
        'var x = package.Foo.methodName();'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertTrue(namespaces_info.IsExtraRequire(token),
                    'Should require the package, not the method specifically.')

  def testIsExtraRequire_notUsed(self):
    """Tests that requires for unused namespaces are extra."""
    input_lines = ['goog.require(\'package.Foo\');']
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertTrue(namespaces_info.IsExtraRequire(token),
                    'Should be extra since it is not used.')

  def testIsExtraRequire_notClosurized(self):
    """Tests that requires of non-closurized namespaces are not extra."""
    input_lines = ['goog.require(\'notclosurized.Foo\');']
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertFalse(namespaces_info.IsExtraRequire(token),
                     'Should not be extra since it is not closurized.')

  def testIsExtraRequire_objectOnClass(self):
    """Tests that requiring an object on a class is extra."""
    input_lines = [
        'goog.require(\'package.Foo.Enum\');',
        'var x = package.Foo.Enum.VALUE1;',
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertTrue(namespaces_info.IsExtraRequire(token),
                    'The whole class, not the object, should be required.');

  def testIsExtraRequire_constantOnClass(self):
    """Tests that requiring a constant on a class is extra."""
    input_lines = [
        'goog.require(\'package.Foo.CONSTANT\');',
        'var x = package.Foo.CONSTANT',
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertTrue(namespaces_info.IsExtraRequire(token),
                    'The class, not the constant, should be required.');

  def testIsExtraRequire_constantNotOnClass(self):
    """Tests that requiring a constant not on a class is OK."""
    input_lines = [
        'goog.require(\'package.subpackage.CONSTANT\');',
        'var x = package.subpackage.CONSTANT',
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertFalse(namespaces_info.IsExtraRequire(token),
                    'Constants can be required except on classes.');

  def testIsExtraRequire_methodNotOnClass(self):
    """Tests that requiring a method not on a class is OK."""
    input_lines = [
        'goog.require(\'package.subpackage.method\');',
        'var x = package.subpackage.method()',
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertFalse(namespaces_info.IsExtraRequire(token),
                    'Methods can be required except on classes.');

  def testIsExtraRequire_defaults(self):
    """Tests that there are no warnings about extra requires for test utils"""
    input_lines = ['goog.require(\'goog.testing.jsunit\');']
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['goog'], [])

    self.assertFalse(namespaces_info.IsExtraRequire(token),
                     'Should not be extra since it is for testing.')

  def testGetMissingProvides_provided(self):
    """Tests that provided functions don't cause a missing provide."""
    input_lines = [
        'goog.provide(\'package.Foo\');',
        'package.Foo = function() {};'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingProvides()))

  def testGetMissingProvides_providedIdentifier(self):
    """Tests that provided identifiers don't cause a missing provide."""
    input_lines = [
        'goog.provide(\'package.Foo.methodName\');',
        'package.Foo.methodName = function() {};'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingProvides()))

  def testGetMissingProvides_providedParentIdentifier(self):
    """Tests that provided identifiers on a class don't cause a missing provide
    on objects attached to that class."""
    input_lines = [
        'goog.provide(\'package.foo.ClassName\');',
        'package.foo.ClassName.methodName = function() {};',
        'package.foo.ClassName.ObjectName = 1;',
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingProvides()))

  def testGetMissingProvides_unprovided(self):
    """Tests that unprovided functions cause a missing provide."""
    input_lines = ['package.Foo = function() {};']
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(1, len(namespaces_info.GetMissingProvides()))
    self.assertTrue('package.Foo' in namespaces_info.GetMissingProvides())

  def testGetMissingProvides_privatefunction(self):
    """Tests that unprovided private functions don't cause a missing provide."""
    input_lines = ['package.Foo_ = function() {};']
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingProvides()))

  def testGetMissingProvides_required(self):
    """Tests that required namespaces don't cause a missing provide."""
    input_lines = [
        'goog.require(\'package.Foo\');',
        'package.Foo.methodName = function() {};'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingProvides()))

  def testGetMissingRequires_required(self):
    """Tests that required namespaces don't cause a missing require."""
    input_lines = [
        'goog.require(\'package.Foo\');',
        'package.Foo();'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingProvides()))

  def testGetMissingRequires_requiredIdentifier(self):
    """Tests that required namespaces satisfy identifiers on that namespace."""
    input_lines = [
        'goog.require(\'package.Foo\');',
        'package.Foo.methodName();'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingProvides()))

  def testGetMissingRequires_requiredParentClass(self):
    """Tests that requiring a parent class of an object is sufficient to prevent
    a missing require on that object."""
    input_lines = [
        'goog.require(\'package.Foo\');',
        'package.Foo.methodName();',
        'package.Foo.methodName(package.Foo.ObjectName);'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingRequires()))

  def testGetMissingRequires_unrequired(self):
    """Tests that unrequired namespaces cause a missing require."""
    input_lines = ['package.Foo();']
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(1, len(namespaces_info.GetMissingRequires()))
    self.assertTrue('package.Foo' in namespaces_info.GetMissingRequires())

  def testGetMissingRequires_provided(self):
    """Tests that provided namespaces satisfy identifiers on that namespace."""
    input_lines = [
        'goog.provide(\'package.Foo\');',
        'package.Foo.methodName();'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingRequires()))

  def testGetMissingRequires_created(self):
    """Tests that created namespaces do not satisfy usage of an identifier."""
    input_lines = [
        'package.Foo = function();',
        'package.Foo.methodName();'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(1, len(namespaces_info.GetMissingRequires()))
    self.assertTrue('package.Foo' in namespaces_info.GetMissingRequires())

  def testGetMissingRequires_createdIdentifier(self):
    """Tests that created identifiers satisfy usage of the identifier."""
    input_lines = [
        'package.Foo.methodName = function();',
        'package.Foo.methodName();'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(0, len(namespaces_info.GetMissingRequires()))

  def testGetMissingRequires_objectOnClass(self):
    """Tests that we should require a class, not the object on the class."""
    input_lines = [
        'goog.require(\'package.Foo.Enum\');',
        'var x = package.Foo.Enum.VALUE1;',
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertEquals(1, len(namespaces_info.GetMissingRequires()),
                    'The whole class, not the object, should be required.');

  def testIsFirstProvide(self):
    """Tests operation of the isFirstProvide method."""
    input_lines = [
        'goog.provide(\'package.Foo\');',
        'package.Foo.methodName();'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = self._GetInitializedNamespacesInfo(token, ['package'], [])

    self.assertTrue(namespaces_info.IsFirstProvide(token))

  def testGetWholeIdentifierString(self):
    """Tests that created identifiers satisfy usage of the identifier."""
    input_lines = [
        'package.Foo.',
        '    veryLong.',
        '    identifier;'
    ]
    token = self._tokenizer.TokenizeFile(input_lines)
    namespaces_info = closurizednamespacesinfo.ClosurizedNamespacesInfo([], [])

    self.assertEquals('package.Foo.veryLong.identifier',
                      namespaces_info._GetWholeIdentifierString(token))
    self.assertEquals(None,
                      namespaces_info._GetWholeIdentifierString(token.next))

  def _GetInitializedNamespacesInfo(self, token, closurized_namespaces,
                                    ignored_extra_namespaces):
    """Returns a namespaces info initialized with the given token stream."""
    namespaces_info = closurizednamespacesinfo.ClosurizedNamespacesInfo(
        closurized_namespaces=closurized_namespaces,
        ignored_extra_namespaces=ignored_extra_namespaces)
    state_tracker = javascriptstatetracker.JavaScriptStateTracker()

    while token:
      namespaces_info.ProcessToken(token, state_tracker)
      token = token.next

    return namespaces_info

  def _GetProvideTokens(self, namespace):
    """Returns a list of tokens for a goog.require of the given namespace."""
    line_text = 'goog.require(\'' + namespace + '\');\n'
    return javascripttokenizer.JavaScriptTokenizer().TokenizeFile([line_text])

  def _GetRequireTokens(self, namespace):
    """Returns a list of tokens for a goog.require of the given namespace."""
    line_text = 'goog.require(\'' + namespace + '\');\n'
    return javascripttokenizer.JavaScriptTokenizer().TokenizeFile([line_text])

if __name__ == '__main__':
  googletest.main()
