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

"""Logic for computing dependency information for closurized JavaScript files.

Closurized JavaScript files express dependencies using goog.require and
goog.provide statements. In order for the linter to detect when a statement is
missing or unnecessary, all identifiers in the JavaScript file must first be
processed to determine if they constitute the creation or usage of a dependency.
"""



from closure_linter import javascripttokens
from closure_linter import tokenutil

# pylint: disable-msg=C6409
TokenType = javascripttokens.JavaScriptTokenType

DEFAULT_EXTRA_NAMESPACES = [
  'goog.testing.asserts',
  'goog.testing.jsunit',
]

class ClosurizedNamespacesInfo(object):
  """Dependency information for closurized JavaScript files.

  Processes token streams for dependency creation or usage and provides logic
  for determining if a given require or provide statement is unnecessary or if
  there are missing require or provide statements.
  """

  def __init__(self, closurized_namespaces, ignored_extra_namespaces):
    """Initializes an instance the ClosurizedNamespacesInfo class.

    Args:
      closurized_namespaces: A list of namespace prefixes that should be
          processed for dependency information. Non-matching namespaces are
          ignored.
      ignored_extra_namespaces: A list of namespaces that should not be reported
          as extra regardless of whether they are actually used.
    """
    self._closurized_namespaces = closurized_namespaces
    self._ignored_extra_namespaces = (ignored_extra_namespaces +
                                      DEFAULT_EXTRA_NAMESPACES)
    self.Reset()

  def Reset(self):
    """Resets the internal state to prepare for processing a new file."""

    # A list of goog.provide tokens in the order they appeared in the file.
    self._provide_tokens = []

    # A list of goog.require tokens in the order they appeared in the file.
    self._require_tokens = []

    # Namespaces that are already goog.provided.
    self._provided_namespaces = []

    # Namespaces that are already goog.required.
    self._required_namespaces = []

    # Note that created_namespaces and used_namespaces contain both namespaces
    # and identifiers because there are many existing cases where a method or
    # constant is provided directly instead of its namespace. Ideally, these
    # two lists would only have to contain namespaces.

    # A list of tuples where the first element is the namespace of an identifier
    # created in the file and the second is the identifier itself.
    self._created_namespaces = []

    # A list of tuples where the first element is the namespace of an identifier
    # used in the file and the second is the identifier itself.
    self._used_namespaces = []

    # A list of seemingly-unnecessary namespaces that are goog.required() and
    # annotated with @suppress {extraRequire}.
    self._suppressed_requires = []

    # A list of goog.provide tokens which are duplicates.
    self._duplicate_provide_tokens = []

    # A list of goog.require tokens which are duplicates.
    self._duplicate_require_tokens = []

    # Whether this file is in a goog.scope. Someday, we may add support
    # for checking scopified namespaces, but for now let's just fail
    # in a more reasonable way.
    self._scopified_file = False

    # TODO(user): Handle the case where there are 2 different requires
    # that can satisfy the same dependency, but only one is necessary.

  def GetProvidedNamespaces(self):
    """Returns the namespaces which are already provided by this file.

    Returns:
      A list of strings where each string is a 'namespace' corresponding to an
      existing goog.provide statement in the file being checked.
    """
    return list(self._provided_namespaces)

  def GetRequiredNamespaces(self):
    """Returns the namespaces which are already required by this file.

    Returns:
      A list of strings where each string is a 'namespace' corresponding to an
      existing goog.require statement in the file being checked.
    """
    return list(self._required_namespaces)

  def IsExtraProvide(self, token):
    """Returns whether the given goog.provide token is unnecessary.

    Args:
      token: A goog.provide token.

    Returns:
      True if the given token corresponds to an unnecessary goog.provide
      statement, otherwise False.
    """
    if self._scopified_file:
      return False

    namespace = tokenutil.Search(token, TokenType.STRING_TEXT).string

    base_namespace = namespace.split('.', 1)[0]
    if base_namespace not in self._closurized_namespaces:
      return False

    if token in self._duplicate_provide_tokens:
      return True

    # TODO(user): There's probably a faster way to compute this.
    for created_namespace, created_identifier in self._created_namespaces:
      if namespace == created_namespace or namespace == created_identifier:
        return False

    return True

  def IsExtraRequire(self, token):
    """Returns whether the given goog.require token is unnecessary.

    Args:
      token: A goog.require token.

    Returns:
      True if the given token corresponds to an unnecessary goog.require
      statement, otherwise False.
    """
    if self._scopified_file:
      return False

    namespace = tokenutil.Search(token, TokenType.STRING_TEXT).string

    base_namespace = namespace.split('.', 1)[0]
    if base_namespace not in self._closurized_namespaces:
      return False

    if namespace in self._ignored_extra_namespaces:
      return False

    if token in self._duplicate_require_tokens:
      return True

    if namespace in self._suppressed_requires:
      return False

    # If the namespace contains a component that is initial caps, then that
    # must be the last component of the namespace.
    parts = namespace.split('.')
    if len(parts) > 1 and parts[-2][0].isupper():
      return True

    # TODO(user): There's probably a faster way to compute this.
    for used_namespace, used_identifier in self._used_namespaces:
      if namespace == used_namespace or namespace == used_identifier:
        return False

    return True

  def GetMissingProvides(self):
    """Returns the set of missing provided namespaces for the current file.

    Returns:
      Returns a set of strings where each string is a namespace that should be
      provided by this file, but is not.
    """
    if self._scopified_file:
      return set()

    missing_provides = set()
    for namespace, identifier in self._created_namespaces:
      if (not self._IsPrivateIdentifier(identifier) and
          namespace not in self._provided_namespaces and
          identifier not in self._provided_namespaces and
          namespace not in self._required_namespaces):
        missing_provides.add(namespace)

    return missing_provides

  def GetMissingRequires(self):
    """Returns the set of missing required namespaces for the current file.

    For each non-private identifier used in the file, find either a
    goog.require, goog.provide or a created identifier that satisfies it.
    goog.require statements can satisfy the identifier by requiring either the
    namespace of the identifier or the identifier itself. goog.provide
    statements can satisfy the identifier by providing the namespace of the
    identifier. A created identifier can only satisfy the used identifier if
    it matches it exactly (necessary since things can be defined on a
    namespace in more than one file). Note that provided namespaces should be
    a subset of created namespaces, but we check both because in some cases we
    can't always detect the creation of the namespace.

    Returns:
      Returns a set of strings where each string is a namespace that should be
      required by this file, but is not.
    """
    if self._scopified_file:
      return set()

    external_dependencies = set(self._required_namespaces)

    # Assume goog namespace is always available.
    external_dependencies.add('goog')

    created_identifiers = set()
    for namespace, identifier in self._created_namespaces:
      created_identifiers.add(identifier)

    missing_requires = set()
    for namespace, identifier in self._used_namespaces:
      if (not self._IsPrivateIdentifier(identifier) and
          namespace not in external_dependencies and
          namespace not in self._provided_namespaces and
          identifier not in external_dependencies and
          identifier not in created_identifiers):
        missing_requires.add(namespace)

    return missing_requires

  def _IsPrivateIdentifier(self, identifier):
    """Returns whether the given identifer is private."""
    pieces = identifier.split('.')
    for piece in pieces:
      if piece.endswith('_'):
        return True
    return False

  def IsFirstProvide(self, token):
    """Returns whether token is the first provide token."""
    return self._provide_tokens and token == self._provide_tokens[0]

  def IsFirstRequire(self, token):
    """Returns whether token is the first require token."""
    return self._require_tokens and token == self._require_tokens[0]

  def IsLastProvide(self, token):
    """Returns whether token is the last provide token."""
    return self._provide_tokens and token == self._provide_tokens[-1]

  def IsLastRequire(self, token):
    """Returns whether token is the last require token."""
    return self._require_tokens and token == self._require_tokens[-1]

  def ProcessToken(self, token, state_tracker):
    """Processes the given token for dependency information.

    Args:
      token: The token to process.
      state_tracker: The JavaScript state tracker.
    """

    # Note that this method is in the critical path for the linter and has been
    # optimized for performance in the following ways:
    # - Tokens are checked by type first to minimize the number of function
    #   calls necessary to determine if action needs to be taken for the token.
    # - The most common tokens types are checked for first.
    # - The number of function calls has been minimized (thus the length of this
    #   function.

    if token.type == TokenType.IDENTIFIER:
      # TODO(user): Consider saving the whole identifier in metadata.
      whole_identifier_string = self._GetWholeIdentifierString(token)
      if whole_identifier_string is None:
        # We only want to process the identifier one time. If the whole string
        # identifier is None, that means this token was part of a multi-token
        # identifier, but it was not the first token of the identifier.
        return

      # In the odd case that a goog.require is encountered inside a function,
      # just ignore it (e.g. dynamic loading in test runners).
      if token.string == 'goog.require' and not state_tracker.InFunction():
        self._require_tokens.append(token)
        namespace = tokenutil.Search(token, TokenType.STRING_TEXT).string
        if namespace in self._required_namespaces:
          self._duplicate_require_tokens.append(token)
        else:
          self._required_namespaces.append(namespace)

        # If there is a suppression for the require, add a usage for it so it
        # gets treated as a regular goog.require (i.e. still gets sorted).
        jsdoc = state_tracker.GetDocComment()
        if jsdoc and ('extraRequire' in jsdoc.suppressions):
          self._suppressed_requires.append(namespace)
          self._AddUsedNamespace(state_tracker, namespace)

      elif token.string == 'goog.provide':
        self._provide_tokens.append(token)
        namespace = tokenutil.Search(token, TokenType.STRING_TEXT).string
        if namespace in self._provided_namespaces:
          self._duplicate_provide_tokens.append(token)
        else:
          self._provided_namespaces.append(namespace)

        # If there is a suppression for the provide, add a creation for it so it
        # gets treated as a regular goog.provide (i.e. still gets sorted).
        jsdoc = state_tracker.GetDocComment()
        if jsdoc and ('extraProvide' in jsdoc.suppressions):
          self._AddCreatedNamespace(state_tracker, namespace)

      elif token.string == 'goog.scope':
        self._scopified_file = True

      else:
        jsdoc = state_tracker.GetDocComment()
        if jsdoc and jsdoc.HasFlag('typedef'):
          self._AddCreatedNamespace(state_tracker, whole_identifier_string,
                                    self.GetClosurizedNamespace(
                                        whole_identifier_string))
        else:
          self._AddUsedNamespace(state_tracker, whole_identifier_string)

    elif token.type == TokenType.SIMPLE_LVALUE:
      identifier = token.values['identifier']
      namespace = self.GetClosurizedNamespace(identifier)
      if state_tracker.InFunction():
        self._AddUsedNamespace(state_tracker, identifier)
      elif namespace and namespace != 'goog':
        self._AddCreatedNamespace(state_tracker, identifier, namespace)

    elif token.type == TokenType.DOC_FLAG:
      flag_type = token.attached_object.flag_type
      is_interface = state_tracker.GetDocComment().HasFlag('interface')
      if flag_type == 'implements' or (flag_type == 'extends' and is_interface):
        # Interfaces should be goog.require'd.
        doc_start = tokenutil.Search(token, TokenType.DOC_START_BRACE)
        interface = tokenutil.Search(doc_start, TokenType.COMMENT)
        self._AddUsedNamespace(state_tracker, interface.string)


  def _GetWholeIdentifierString(self, token):
    """Returns the whole identifier string for the given token.

    Checks the tokens after the current one to see if the token is one in a
    sequence of tokens which are actually just one identifier (i.e. a line was
    wrapped in the middle of an identifier).

    Args:
      token: The token to check.

    Returns:
      The whole identifier string or None if this token is not the first token
      in a multi-token identifier.
    """
    result = ''

    # Search backward to determine if this token is the first token of the
    # identifier. If it is not the first token, return None to signal that this
    # token should be ignored.
    prev_token = token.previous
    while prev_token:
      if (prev_token.IsType(TokenType.IDENTIFIER) or
          prev_token.IsType(TokenType.NORMAL) and prev_token.string == '.'):
        return None
      elif (not prev_token.IsType(TokenType.WHITESPACE) and
            not prev_token.IsAnyType(TokenType.COMMENT_TYPES)):
        break
      prev_token = prev_token.previous

    # Search forward to find other parts of this identifier separated by white
    # space.
    next_token = token
    while next_token:
      if (next_token.IsType(TokenType.IDENTIFIER) or
          next_token.IsType(TokenType.NORMAL) and next_token.string == '.'):
        result += next_token.string
      elif (not next_token.IsType(TokenType.WHITESPACE) and
            not next_token.IsAnyType(TokenType.COMMENT_TYPES)):
        break
      next_token = next_token.next

    return result

  def _AddCreatedNamespace(self, state_tracker, identifier, namespace=None):
    """Adds the namespace of an identifier to the list of created namespaces.

    If the identifier is annotated with a 'missingProvide' suppression, it is
    not added.

    Args:
      state_tracker: The JavaScriptStateTracker instance.
      identifier: The identifier to add.
      namespace: The namespace of the identifier or None if the identifier is
          also the namespace.
    """
    if not namespace:
      namespace = identifier

    jsdoc = state_tracker.GetDocComment()
    if jsdoc and 'missingProvide' in jsdoc.suppressions:
      return

    self._created_namespaces.append([namespace, identifier])

  def _AddUsedNamespace(self, state_tracker, identifier):
    """Adds the namespace of an identifier to the list of used namespaces.

    If the identifier is annotated with a 'missingRequire' suppression, it is
    not added.

    Args:
      state_tracker: The JavaScriptStateTracker instance.
      identifier: An identifier which has been used.
    """
    jsdoc = state_tracker.GetDocComment()
    if jsdoc and 'missingRequire' in jsdoc.suppressions:
      return

    namespace = self.GetClosurizedNamespace(identifier)
    if namespace:
      self._used_namespaces.append([namespace, identifier])

  def GetClosurizedNamespace(self, identifier):
    """Given an identifier, returns the namespace that identifier is from.

    Args:
      identifier: The identifier to extract a namespace from.

    Returns:
      The namespace the given identifier resides in, or None if one could not
      be found.
    """
    if identifier.startswith('goog.global'):
      # Ignore goog.global, since it is, by definition, global.
      return None

    parts = identifier.split('.')
    for namespace in self._closurized_namespaces:
      if not identifier.startswith(namespace + '.'):
        continue

      last_part = parts[-1]
      if not last_part:
        # TODO(robbyw): Handle this: it's a multi-line identifier.
        return None

      # The namespace for a class is the shortest prefix ending in a class
      # name, which starts with a capital letter but is not a capitalized word.
      #
      # We ultimately do not want to allow requiring or providing of inner
      # classes/enums.  Instead, a file should provide only the top-level class
      # and users should require only that.
      namespace = []
      for part in parts:
        if part == 'prototype' or part.isupper():
          return '.'.join(namespace)
        namespace.append(part)
        if part[0].isupper():
          return '.'.join(namespace)

      # At this point, we know there's no class or enum, so the namespace is
      # just the identifier with the last part removed. With the exception of
      # apply, inherits, and call, which should also be stripped.
      if parts[-1] in ('apply', 'inherits', 'call'):
        parts.pop()
      parts.pop()

      # If the last part ends with an underscore, it is a private variable,
      # method, or enum. The namespace is whatever is before it.
      if parts and parts[-1].endswith('_'):
        parts.pop()

      return '.'.join(parts)

    return None
