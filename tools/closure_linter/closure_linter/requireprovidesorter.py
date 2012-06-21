#!/usr/bin/env python
#
# Copyright 2011 The Closure Linter Authors. All Rights Reserved.
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

"""Contains logic for sorting goog.provide and goog.require statements.

Closurized JavaScript files use goog.provide and goog.require statements at the
top of the file to manage dependencies. These statements should be sorted
alphabetically, however, it is common for them to be accompanied by inline
comments or suppression annotations. In order to sort these statements without
disrupting their comments and annotations, the association between statements
and comments/annotations must be maintained while sorting.

  RequireProvideSorter: Handles checking/fixing of provide/require statements.
"""



from closure_linter import javascripttokens
from closure_linter import tokenutil

# Shorthand
Type = javascripttokens.JavaScriptTokenType


class RequireProvideSorter(object):
  """Checks for and fixes alphabetization of provide and require statements.

  When alphabetizing, comments on the same line or comments directly above a
  goog.provide or goog.require statement are associated with that statement and
  stay with the statement as it gets sorted.
  """

  def CheckProvides(self, token):
    """Checks alphabetization of goog.provide statements.

    Iterates over tokens in given token stream, identifies goog.provide tokens,
    and checks that they occur in alphabetical order by the object being
    provided.

    Args:
      token: A token in the token stream before any goog.provide tokens.

    Returns:
      A tuple containing the first provide token in the token stream and a list
      of provided objects sorted alphabetically. For example:

      (JavaScriptToken, ['object.a', 'object.b', ...])

      None is returned if all goog.provide statements are already sorted.
    """
    provide_tokens = self._GetRequireOrProvideTokens(token, 'goog.provide')
    provide_strings = self._GetRequireOrProvideTokenStrings(provide_tokens)
    sorted_provide_strings = sorted(provide_strings)
    if provide_strings != sorted_provide_strings:
      return [provide_tokens[0], sorted_provide_strings]
    return None

  def CheckRequires(self, token):
    """Checks alphabetization of goog.require statements.

    Iterates over tokens in given token stream, identifies goog.require tokens,
    and checks that they occur in alphabetical order by the dependency being
    required.

    Args:
      token: A token in the token stream before any goog.require tokens.

    Returns:
      A tuple containing the first require token in the token stream and a list
      of required dependencies sorted alphabetically. For example:

      (JavaScriptToken, ['object.a', 'object.b', ...])

      None is returned if all goog.require statements are already sorted.
    """
    require_tokens = self._GetRequireOrProvideTokens(token, 'goog.require')
    require_strings = self._GetRequireOrProvideTokenStrings(require_tokens)
    sorted_require_strings = sorted(require_strings)
    if require_strings != sorted_require_strings:
      return (require_tokens[0], sorted_require_strings)
    return None

  def FixProvides(self, token):
    """Sorts goog.provide statements in the given token stream alphabetically.

    Args:
      token: The first token in the token stream.
    """
    self._FixProvidesOrRequires(
        self._GetRequireOrProvideTokens(token, 'goog.provide'))

  def FixRequires(self, token):
    """Sorts goog.require statements in the given token stream alphabetically.

    Args:
      token: The first token in the token stream.
    """
    self._FixProvidesOrRequires(
        self._GetRequireOrProvideTokens(token, 'goog.require'))

  def _FixProvidesOrRequires(self, tokens):
    """Sorts goog.provide or goog.require statements.

    Args:
      tokens: A list of goog.provide or goog.require tokens in the order they
              appear in the token stream. i.e. the first token in this list must
              be the first goog.provide or goog.require token.
    """
    strings = self._GetRequireOrProvideTokenStrings(tokens)
    sorted_strings = sorted(strings)

    # Make a separate pass to remove any blank lines between goog.require/
    # goog.provide tokens.
    first_token = tokens[0]
    last_token = tokens[-1]
    i = last_token
    while i != first_token:
      if i.type is Type.BLANK_LINE:
        tokenutil.DeleteToken(i)
      i = i.previous

    # A map from required/provided object name to tokens that make up the line
    # it was on, including any comments immediately before it or after it on the
    # same line.
    tokens_map = self._GetTokensMap(tokens)

    # Iterate over the map removing all tokens.
    for name in tokens_map:
      tokens_to_delete = tokens_map[name]
      for i in tokens_to_delete:
        tokenutil.DeleteToken(i)

    # Re-add all tokens in the map in alphabetical order.
    insert_after = tokens[0].previous
    for string in sorted_strings:
      for i in tokens_map[string]:
        tokenutil.InsertTokenAfter(i, insert_after)
        insert_after = i

  def _GetRequireOrProvideTokens(self, token, token_string):
    """Gets all goog.provide or goog.require tokens in the given token stream.

    Args:
      token: The first token in the token stream.
      token_string: One of 'goog.provide' or 'goog.require' to indicate which
                    tokens to find.

    Returns:
      A list of goog.provide or goog.require tokens in the order they appear in
      the token stream.
    """
    tokens = []
    while token:
      if token.type == Type.IDENTIFIER:
        if token.string == token_string:
          tokens.append(token)
        elif token.string not in ['goog.require', 'goog.provide']:
          # The goog.provide and goog.require identifiers are at the top of the
          # file. So if any other identifier is encountered, return.
          break
      token = token.next

    return tokens

  def _GetRequireOrProvideTokenStrings(self, tokens):
    """Gets a list of strings corresponding to the given list of tokens.

    The string will be the next string in the token stream after each token in
    tokens. This is used to find the object being provided/required by a given
    goog.provide or goog.require token.

    Args:
      tokens: A list of goog.provide or goog.require tokens.

    Returns:
      A list of object names that are being provided or required by the given
      list of tokens. For example:

      ['object.a', 'object.c', 'object.b']
    """
    token_strings = []
    for token in tokens:
      name = tokenutil.Search(token, Type.STRING_TEXT).string
      token_strings.append(name)
    return token_strings

  def _GetTokensMap(self, tokens):
    """Gets a map from object name to tokens associated with that object.

    Starting from the goog.provide/goog.require token, searches backwards in the
    token stream for any lines that start with a comment. These lines are
    associated with the goog.provide/goog.require token. Also associates any
    tokens on the same line as the goog.provide/goog.require token with that
    token.

    Args:
      tokens: A list of goog.provide or goog.require tokens.

    Returns:
      A dictionary that maps object names to the tokens associated with the
      goog.provide or goog.require of that object name. For example:

      {
        'object.a': [JavaScriptToken, JavaScriptToken, ...],
        'object.b': [...]
      }

      The list of tokens includes any comment lines above the goog.provide or
      goog.require statement and everything after the statement on the same
      line. For example, all of the following would be associated with
      'object.a':

      /** @suppress {extraRequire} */
      goog.require('object.a'); // Some comment.
    """
    tokens_map = {}
    for token in tokens:
      object_name = tokenutil.Search(token, Type.STRING_TEXT).string
      # If the previous line starts with a comment, presume that the comment
      # relates to the goog.require or goog.provide and keep them together when
      # sorting.
      first_token = token
      previous_first_token = tokenutil.GetFirstTokenInPreviousLine(first_token)
      while previous_first_token.IsAnyType(Type.COMMENT_TYPES):
        first_token = previous_first_token
        previous_first_token = tokenutil.GetFirstTokenInPreviousLine(
            first_token)

      # Find the last token on the line.
      last_token = tokenutil.GetLastTokenInSameLine(token)

      all_tokens = self._GetTokenList(first_token, last_token)
      tokens_map[object_name] = all_tokens
    return tokens_map

  def _GetTokenList(self, first_token, last_token):
    """Gets a list of all tokens from first_token to last_token, inclusive.

    Args:
      first_token: The first token to get.
      last_token: The last token to get.

    Returns:
      A list of all tokens between first_token and last_token, including both
      first_token and last_token.

    Raises:
      Exception: If the token stream ends before last_token is reached.
    """
    token_list = []
    token = first_token
    while token != last_token:
      if not token:
        raise Exception('ran out of tokens')
      token_list.append(token)
      token = token.next
    token_list.append(last_token)

    return token_list
