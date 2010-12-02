#!/usr/bin/env python
#
# Copyright 2008 The Closure Linter Authors. All Rights Reserved.
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

"""Parser for JavaScript files."""



from closure_linter import javascripttokens
from closure_linter import statetracker
from closure_linter import tokenutil

# Shorthand
Type = javascripttokens.JavaScriptTokenType


class JsDocFlag(statetracker.DocFlag):
  """Javascript doc flag object.

  Attribute:
    flag_type: param, return, define, type, etc.
    flag_token: The flag token.
    type_start_token: The first token specifying the flag JS type,
      including braces.
    type_end_token: The last token specifying the flag JS type,
      including braces.
    type: The JavaScript type spec.
    name_token: The token specifying the flag name.
    name: The flag name
    description_start_token: The first token in the description.
    description_end_token: The end token in the description.
    description: The description.
  """

  # Please keep these lists alphabetized.

  # Some projects use the following extensions to JsDoc.
  # TODO(robbyw): determine which of these, if any, should be illegal.
  EXTENDED_DOC = frozenset([
      'class', 'code', 'desc', 'final', 'hidden', 'inheritDoc', 'link',
      'protected', 'notypecheck', 'throws'])

  LEGAL_DOC = EXTENDED_DOC | statetracker.DocFlag.LEGAL_DOC

  def __init__(self, flag_token):
    """Creates the JsDocFlag object and attaches it to the given start token.

    Args:
      flag_token: The starting token of the flag.
    """
    statetracker.DocFlag.__init__(self, flag_token)


class JavaScriptStateTracker(statetracker.StateTracker):
  """JavaScript state tracker.

  Inherits from the core EcmaScript StateTracker adding extra state tracking
  functionality needed for JavaScript.
  """

  def __init__(self, closurized_namespaces=''):
    """Initializes a JavaScript token stream state tracker.

    Args:
      closurized_namespaces: An optional list of namespace prefixes used for
          testing of goog.provide/require.
    """
    statetracker.StateTracker.__init__(self, JsDocFlag)
    self.__closurized_namespaces = closurized_namespaces

  def Reset(self):
    """Resets the state tracker to prepare for processing a new page."""
    super(JavaScriptStateTracker, self).Reset()

    self.__goog_require_tokens = []
    self.__goog_provide_tokens = []
    self.__provided_namespaces = set()
    self.__used_namespaces = []

  def InTopLevel(self):
    """Compute whether we are at the top level in the class.

    This function call is language specific.  In some languages like
    JavaScript, a function is top level if it is not inside any parenthesis.
    In languages such as ActionScript, a function is top level if it is directly
    within a class.

    Returns:
      Whether we are at the top level in the class.
    """
    return not self.InParentheses()

  def GetGoogRequireTokens(self):
    """Returns list of require tokens."""
    return self.__goog_require_tokens

  def GetGoogProvideTokens(self):
    """Returns list of provide tokens."""
    return self.__goog_provide_tokens

  def GetProvidedNamespaces(self):
    """Returns list of provided namespaces."""
    return self.__provided_namespaces

  def GetUsedNamespaces(self):
    """Returns list of used namespaces, is a list of sequences."""
    return self.__used_namespaces

  def GetBlockType(self, token):
    """Determine the block type given a START_BLOCK token.

    Code blocks come after parameters, keywords  like else, and closing parens.

    Args:
      token: The current token. Can be assumed to be type START_BLOCK
    Returns:
      Code block type for current token.
    """
    last_code = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES, None,
                                       True)
    if last_code.type in (Type.END_PARAMETERS, Type.END_PAREN,
                          Type.KEYWORD) and not last_code.IsKeyword('return'):
      return self.CODE
    else:
      return self.OBJECT_LITERAL

  def HandleToken(self, token, last_non_space_token):
    """Handles the given token and updates state.

    Args:
      token: The token to handle.
      last_non_space_token:
    """
    super(JavaScriptStateTracker, self).HandleToken(token,
                                                    last_non_space_token)

    if token.IsType(Type.IDENTIFIER):
      if token.string == 'goog.require':
        class_token = tokenutil.Search(token, Type.STRING_TEXT)
        self.__goog_require_tokens.append(class_token)

      elif token.string == 'goog.provide':
        class_token = tokenutil.Search(token, Type.STRING_TEXT)
        self.__goog_provide_tokens.append(class_token)

      elif self.__closurized_namespaces:
        self.__AddUsedNamespace(token.string)
    if token.IsType(Type.SIMPLE_LVALUE) and not self.InFunction():
      identifier = token.values['identifier']

      if self.__closurized_namespaces:
        namespace = self.GetClosurizedNamespace(identifier)
        if namespace and identifier == namespace:
          self.__provided_namespaces.add(namespace)
    if (self.__closurized_namespaces and
        token.IsType(Type.DOC_FLAG) and
        token.attached_object.flag_type == 'implements'):
      # Interfaces should be goog.require'd.
      doc_start = tokenutil.Search(token, Type.DOC_START_BRACE)
      interface = tokenutil.Search(doc_start, Type.COMMENT)
      self.__AddUsedNamespace(interface.string)

  def __AddUsedNamespace(self, identifier):
    """Adds the namespace of an identifier to the list of used namespaces.
    
    Args:
      identifier: An identifier which has been used.
    """
    namespace = self.GetClosurizedNamespace(identifier)

    if namespace:
      # We add token.string as a 'namespace' as it is something that could
      # potentially be provided to satisfy this dependency.
      self.__used_namespaces.append([namespace, identifier])

  def GetClosurizedNamespace(self, identifier):
    """Given an identifier, returns the namespace that identifier is from.

    Args:
      identifier: The identifier to extract a namespace from.

    Returns:
      The namespace the given identifier resides in, or None if one could not
      be found.
    """
    parts = identifier.split('.')
    for part in parts:
      if part.endswith('_'):
        # Ignore private variables / inner classes.
        return None

    if identifier.startswith('goog.global'):
      # Ignore goog.global, since it is, by definition, global.
      return None

    for namespace in self.__closurized_namespaces:
      if identifier.startswith(namespace + '.'):
        last_part = parts[-1]
        if not last_part:
          # TODO(robbyw): Handle this: it's a multi-line identifier.
          return None

        if last_part in ('apply', 'inherits', 'call'):
          # Calling one of Function's methods usually indicates use of a
          # superclass.
          parts.pop()
          last_part = parts[-1]

        for i in xrange(1, len(parts)):
          part = parts[i]
          if part.isupper():
            # If an identifier is of the form foo.bar.BAZ.x or foo.bar.BAZ,
            # the namespace is foo.bar.
            return '.'.join(parts[:i])
          if part == 'prototype':
            # If an identifier is of the form foo.bar.prototype.x, the
            # namespace is foo.bar.
            return '.'.join(parts[:i])

        if last_part.isupper() or not last_part[0].isupper():
          # Strip off the last part of an enum or constant reference.
          parts.pop()

        return '.'.join(parts)

    return None
