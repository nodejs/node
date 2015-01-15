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



import re

from closure_linter import javascripttokens
from closure_linter import tokenutil

# pylint: disable=g-bad-name
TokenType = javascripttokens.JavaScriptTokenType

DEFAULT_EXTRA_NAMESPACES = [
    'goog.testing.asserts',
    'goog.testing.jsunit',
]


class UsedNamespace(object):
  """A type for information about a used namespace."""

  def __init__(self, namespace, identifier, token, alias_definition):
    """Initializes the instance.

    Args:
      namespace: the namespace of an identifier used in the file
      identifier: the complete identifier
      token: the token that uses the namespace
      alias_definition: a boolean stating whether the namespace is only to used
          for an alias definition and should not be required.
    """
    self.namespace = namespace
    self.identifier = identifier
    self.token = token
    self.alias_definition = alias_definition

  def GetLine(self):
    return self.token.line_number

  def __repr__(self):
    return 'UsedNamespace(%s)' % ', '.join(
        ['%s=%s' % (k, repr(v)) for k, v in self.__dict__.iteritems()])


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
    # created in the file, the second is the identifier itself and the third is
    # the line number where it's created.
    self._created_namespaces = []

    # A list of UsedNamespace instances.
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
    return set(self._provided_namespaces)

  def GetRequiredNamespaces(self):
    """Returns the namespaces which are already required by this file.

    Returns:
      A list of strings where each string is a 'namespace' corresponding to an
      existing goog.require statement in the file being checked.
    """
    return set(self._required_namespaces)

  def IsExtraProvide(self, token):
    """Returns whether the given goog.provide token is unnecessary.

    Args:
      token: A goog.provide token.

    Returns:
      True if the given token corresponds to an unnecessary goog.provide
      statement, otherwise False.
    """
    namespace = tokenutil.GetStringAfterToken(token)

    if self.GetClosurizedNamespace(namespace) is None:
      return False

    if token in self._duplicate_provide_tokens:
      return True

    # TODO(user): There's probably a faster way to compute this.
    for created_namespace, created_identifier, _ in self._created_namespaces:
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
    namespace = tokenutil.GetStringAfterToken(token)

    if self.GetClosurizedNamespace(namespace) is None:
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
    for ns in self._used_namespaces:
      if (not ns.alias_definition and (
          namespace == ns.namespace or namespace == ns.identifier)):
        return False

    return True

  def GetMissingProvides(self):
    """Returns the dict of missing provided namespaces for the current file.

    Returns:
      Returns a dictionary of key as string and value as integer where each
      string(key) is a namespace that should be provided by this file, but is
      not and integer(value) is first line number where it's defined.
    """
    missing_provides = dict()
    for namespace, identifier, line_number in self._created_namespaces:
      if (not self._IsPrivateIdentifier(identifier) and
          namespace not in self._provided_namespaces and
          identifier not in self._provided_namespaces and
          namespace not in self._required_namespaces and
          namespace not in missing_provides):
        missing_provides[namespace] = line_number

    return missing_provides

  def GetMissingRequires(self):
    """Returns the dict of missing required namespaces for the current file.

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
      Returns a dictionary of key as string and value integer where each
      string(key) is a namespace that should be required by this file, but is
      not and integer(value) is first line number where it's used.
    """
    external_dependencies = set(self._required_namespaces)

    # Assume goog namespace is always available.
    external_dependencies.add('goog')
    # goog.module is treated as a builtin, too (for goog.module.get).
    external_dependencies.add('goog.module')

    created_identifiers = set()
    for unused_namespace, identifier, unused_line_number in (
        self._created_namespaces):
      created_identifiers.add(identifier)

    missing_requires = dict()
    illegal_alias_statements = dict()

    def ShouldRequireNamespace(namespace, identifier):
      """Checks if a namespace would normally be required."""
      return (
          not self._IsPrivateIdentifier(identifier) and
          namespace not in external_dependencies and
          namespace not in self._provided_namespaces and
          identifier not in external_dependencies and
          identifier not in created_identifiers and
          namespace not in missing_requires)

    # First check all the used identifiers where we know that their namespace
    # needs to be provided (unless they are optional).
    for ns in self._used_namespaces:
      namespace = ns.namespace
      identifier = ns.identifier
      if (not ns.alias_definition and
          ShouldRequireNamespace(namespace, identifier)):
        missing_requires[namespace] = ns.GetLine()

    # Now that all required namespaces are known, we can check if the alias
    # definitions (that are likely being used for typeannotations that don't
    # need explicit goog.require statements) are already covered. If not
    # the user shouldn't use the alias.
    for ns in self._used_namespaces:
      if (not ns.alias_definition or
          not ShouldRequireNamespace(ns.namespace, ns.identifier)):
        continue
      if self._FindNamespace(ns.identifier, self._provided_namespaces,
                             created_identifiers, external_dependencies,
                             missing_requires):
        continue
      namespace = ns.identifier.rsplit('.', 1)[0]
      illegal_alias_statements[namespace] = ns.token

    return missing_requires, illegal_alias_statements

  def _FindNamespace(self, identifier, *namespaces_list):
    """Finds the namespace of an identifier given a list of other namespaces.

    Args:
      identifier: An identifier whose parent needs to be defined.
          e.g. for goog.bar.foo we search something that provides
          goog.bar.
      *namespaces_list: var args of iterables of namespace identifiers
    Returns:
      The namespace that the given identifier is part of or None.
    """
    identifier = identifier.rsplit('.', 1)[0]
    identifier_prefix = identifier + '.'
    for namespaces in namespaces_list:
      for namespace in namespaces:
        if namespace == identifier or namespace.startswith(identifier_prefix):
          return namespace
    return None

  def _IsPrivateIdentifier(self, identifier):
    """Returns whether the given identifier is private."""
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
      whole_identifier_string = tokenutil.GetIdentifierForToken(token)
      if whole_identifier_string is None:
        # We only want to process the identifier one time. If the whole string
        # identifier is None, that means this token was part of a multi-token
        # identifier, but it was not the first token of the identifier.
        return

      # In the odd case that a goog.require is encountered inside a function,
      # just ignore it (e.g. dynamic loading in test runners).
      if token.string == 'goog.require' and not state_tracker.InFunction():
        self._require_tokens.append(token)
        namespace = tokenutil.GetStringAfterToken(token)
        if namespace in self._required_namespaces:
          self._duplicate_require_tokens.append(token)
        else:
          self._required_namespaces.append(namespace)

        # If there is a suppression for the require, add a usage for it so it
        # gets treated as a regular goog.require (i.e. still gets sorted).
        if self._HasSuppression(state_tracker, 'extraRequire'):
          self._suppressed_requires.append(namespace)
          self._AddUsedNamespace(state_tracker, namespace, token)

      elif token.string == 'goog.provide':
        self._provide_tokens.append(token)
        namespace = tokenutil.GetStringAfterToken(token)
        if namespace in self._provided_namespaces:
          self._duplicate_provide_tokens.append(token)
        else:
          self._provided_namespaces.append(namespace)

        # If there is a suppression for the provide, add a creation for it so it
        # gets treated as a regular goog.provide (i.e. still gets sorted).
        if self._HasSuppression(state_tracker, 'extraProvide'):
          self._AddCreatedNamespace(state_tracker, namespace, token.line_number)

      elif token.string == 'goog.scope':
        self._scopified_file = True

      elif token.string == 'goog.setTestOnly':

        # Since the message is optional, we don't want to scan to later lines.
        for t in tokenutil.GetAllTokensInSameLine(token):
          if t.type == TokenType.STRING_TEXT:
            message = t.string

            if re.match(r'^\w+(\.\w+)+$', message):
              # This looks like a namespace. If it's a Closurized namespace,
              # consider it created.
              base_namespace = message.split('.', 1)[0]
              if base_namespace in self._closurized_namespaces:
                self._AddCreatedNamespace(state_tracker, message,
                                          token.line_number)

            break
      else:
        jsdoc = state_tracker.GetDocComment()
        if token.metadata and token.metadata.aliased_symbol:
          whole_identifier_string = token.metadata.aliased_symbol
        elif (token.string == 'goog.module.get' and
              not self._HasSuppression(state_tracker, 'extraRequire')):
          # Cannot use _AddUsedNamespace as this is not an identifier, but
          # already the entire namespace that's required.
          namespace = tokenutil.GetStringAfterToken(token)
          namespace = UsedNamespace(namespace, namespace, token,
                                    alias_definition=False)
          self._used_namespaces.append(namespace)
        if jsdoc and jsdoc.HasFlag('typedef'):
          self._AddCreatedNamespace(state_tracker, whole_identifier_string,
                                    token.line_number,
                                    namespace=self.GetClosurizedNamespace(
                                        whole_identifier_string))
        else:
          is_alias_definition = (token.metadata and
                                 token.metadata.is_alias_definition)
          self._AddUsedNamespace(state_tracker, whole_identifier_string,
                                 token, is_alias_definition)

    elif token.type == TokenType.SIMPLE_LVALUE:
      identifier = token.values['identifier']
      start_token = tokenutil.GetIdentifierStart(token)
      if start_token and start_token != token:
        # Multi-line identifier being assigned. Get the whole identifier.
        identifier = tokenutil.GetIdentifierForToken(start_token)
      else:
        start_token = token
      # If an alias is defined on the start_token, use it instead.
      if (start_token and
          start_token.metadata and
          start_token.metadata.aliased_symbol and
          not start_token.metadata.is_alias_definition):
        identifier = start_token.metadata.aliased_symbol

      if identifier:
        namespace = self.GetClosurizedNamespace(identifier)
        if state_tracker.InFunction():
          self._AddUsedNamespace(state_tracker, identifier, token)
        elif namespace and namespace != 'goog':
          self._AddCreatedNamespace(state_tracker, identifier,
                                    token.line_number, namespace=namespace)

    elif token.type == TokenType.DOC_FLAG:
      flag = token.attached_object
      flag_type = flag.flag_type
      if flag and flag.HasType() and flag.jstype:
        is_interface = state_tracker.GetDocComment().HasFlag('interface')
        if flag_type == 'implements' or (flag_type == 'extends'
                                         and is_interface):
          identifier = flag.jstype.alias or flag.jstype.identifier
          self._AddUsedNamespace(state_tracker, identifier, token)
          # Since we process doctypes only for implements and extends, the
          # type is a simple one and we don't need any iteration for subtypes.

  def _AddCreatedNamespace(self, state_tracker, identifier, line_number,
                           namespace=None):
    """Adds the namespace of an identifier to the list of created namespaces.

    If the identifier is annotated with a 'missingProvide' suppression, it is
    not added.

    Args:
      state_tracker: The JavaScriptStateTracker instance.
      identifier: The identifier to add.
      line_number: Line number where namespace is created.
      namespace: The namespace of the identifier or None if the identifier is
          also the namespace.
    """
    if not namespace:
      namespace = identifier

    if self._HasSuppression(state_tracker, 'missingProvide'):
      return

    self._created_namespaces.append([namespace, identifier, line_number])

  def _AddUsedNamespace(self, state_tracker, identifier, token,
                        is_alias_definition=False):
    """Adds the namespace of an identifier to the list of used namespaces.

    If the identifier is annotated with a 'missingRequire' suppression, it is
    not added.

    Args:
      state_tracker: The JavaScriptStateTracker instance.
      identifier: An identifier which has been used.
      token: The token in which the namespace is used.
      is_alias_definition: If the used namespace is part of an alias_definition.
          Aliased symbols need their parent namespace to be available, if it is
          not yet required through another symbol, an error will be thrown.
    """
    if self._HasSuppression(state_tracker, 'missingRequire'):
      return

    namespace = self.GetClosurizedNamespace(identifier)
    # b/5362203 If its a variable in scope then its not a required namespace.
    if namespace and not state_tracker.IsVariableInScope(namespace):
      namespace = UsedNamespace(namespace, identifier, token,
                                is_alias_definition)
      self._used_namespaces.append(namespace)

  def _HasSuppression(self, state_tracker, suppression):
    jsdoc = state_tracker.GetDocComment()
    return jsdoc and suppression in jsdoc.suppressions

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
