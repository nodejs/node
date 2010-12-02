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

"""Methods for checking JS files for common style guide violations.

These style guide violations should only apply to JavaScript and not an Ecma
scripting languages.
"""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)',
              'jacobr@google.com (Jacob Richman)')

import gflags as flags
from closure_linter import ecmalintrules
from closure_linter import errors
from closure_linter import javascripttokenizer
from closure_linter import javascripttokens
from closure_linter import tokenutil
from closure_linter.common import error
from closure_linter.common import position

FLAGS = flags.FLAGS
flags.DEFINE_list('closurized_namespaces', '',
                  'Namespace prefixes, used for testing of'
                  'goog.provide/require')
flags.DEFINE_list('ignored_extra_namespaces', '',
                  'Fully qualified namespaces that should be not be reported '
                  'as extra by the linter.')

# Shorthand
Error = error.Error
Position = position.Position
Type = javascripttokens.JavaScriptTokenType


class JavaScriptLintRules(ecmalintrules.EcmaScriptLintRules):
  """JavaScript lint rules that catch JavaScript specific style errors."""

  def HandleMissingParameterDoc(self, token, param_name):
    """Handle errors associated with a parameter missing a param tag."""
    self._HandleError(errors.MISSING_PARAMETER_DOCUMENTATION,
                      'Missing docs for parameter: "%s"' % param_name, token)

  def __ContainsRecordType(self, token):
    """Check whether the given token contains a record type.

    Args:
      token: The token being checked
    """
    # If we see more than one left-brace in the string of an annotation token,
    # then there's a record type in there.
    return (token and token.type == Type.DOC_FLAG and
        token.attached_object.type is not None and
        token.attached_object.type.find('{') != token.string.rfind('{'))


  def CheckToken(self, token, state):
    """Checks a token, given the current parser_state, for warnings and errors.

    Args:
      token: The current token under consideration
      state: parser_state object that indicates the current state in the page
    """
    if self.__ContainsRecordType(token):
      # We should bail out and not emit any warnings for this annotation.
      # TODO(nicksantos): Support record types for real.
      state.GetDocComment().Invalidate()
      return

    # Call the base class's CheckToken function.
    super(JavaScriptLintRules, self).CheckToken(token, state)

    # Store some convenience variables
    first_in_line = token.IsFirstInLine()
    last_in_line = token.IsLastInLine()
    type = token.type

    if type == Type.DOC_FLAG:
      flag = token.attached_object

      if flag.flag_type == 'param' and flag.name_token is not None:
        self._CheckForMissingSpaceBeforeToken(
            token.attached_object.name_token)

      if flag.flag_type in state.GetDocFlag().HAS_TYPE:
        # Check for both missing type token and empty type braces '{}'
        # Missing suppress types are reported separately and we allow enums
        # without types.
        if (flag.flag_type not in ('suppress', 'enum') and
            (flag.type == None or flag.type == '' or flag.type.isspace())):
          self._HandleError(errors.MISSING_JSDOC_TAG_TYPE,
                            'Missing type in %s tag' % token.string, token)

        elif flag.name_token and flag.type_end_token and tokenutil.Compare(
            flag.type_end_token, flag.name_token) > 0:
          self._HandleError(
              errors.OUT_OF_ORDER_JSDOC_TAG_TYPE,
              'Type should be immediately after %s tag' % token.string,
              token)

    elif type == Type.DOUBLE_QUOTE_STRING_START:
      next = token.next
      while next.type == Type.STRING_TEXT:
        if javascripttokenizer.JavaScriptTokenizer.SINGLE_QUOTE.search(
            next.string):
          break
        next = next.next
      else:
        self._HandleError(
            errors.UNNECESSARY_DOUBLE_QUOTED_STRING,
            'Single-quoted string preferred over double-quoted string.',
            token,
            Position.All(token.string))

    elif type == Type.END_DOC_COMMENT:
      if (FLAGS.strict and not self._is_html and state.InTopLevel() and
          not state.InBlock()):

        # Check if we're in a fileoverview or constructor JsDoc.
        doc_comment = state.GetDocComment()
        is_constructor = (doc_comment.HasFlag('constructor') or
            doc_comment.HasFlag('interface'))
        is_file_overview = doc_comment.HasFlag('fileoverview')

        # If the comment is not a file overview, and it does not immediately
        # precede some code, skip it.
        # NOTE: The tokenutil methods are not used here because of their
        # behavior at the top of a file.
        next = token.next
        if (not next or
            (not is_file_overview and next.type in Type.NON_CODE_TYPES)):
          return

        # Find the start of this block (include comments above the block, unless
        # this is a file overview).
        block_start = doc_comment.start_token
        if not is_file_overview:
          token = block_start.previous
          while token and token.type in Type.COMMENT_TYPES:
            block_start = token
            token = token.previous

        # Count the number of blank lines before this block.
        blank_lines = 0
        token = block_start.previous
        while token and token.type in [Type.WHITESPACE, Type.BLANK_LINE]:
          if token.type == Type.BLANK_LINE:
            # A blank line.
            blank_lines += 1
          elif token.type == Type.WHITESPACE and not token.line.strip():
            # A line with only whitespace on it.
            blank_lines += 1
          token = token.previous

        # Log errors.
        error_message = False
        expected_blank_lines = 0

        if is_file_overview and blank_lines == 0:
          error_message = 'Should have a blank line before a file overview.'
          expected_blank_lines = 1
        elif is_constructor and blank_lines != 3:
          error_message = ('Should have 3 blank lines before a constructor/'
              'interface.')
          expected_blank_lines = 3
        elif not is_file_overview and not is_constructor and blank_lines != 2:
          error_message = 'Should have 2 blank lines between top-level blocks.'
          expected_blank_lines = 2

        if error_message:
          self._HandleError(errors.WRONG_BLANK_LINE_COUNT, error_message,
              block_start, Position.AtBeginning(),
              expected_blank_lines - blank_lines)

    elif type == Type.END_BLOCK:
      if state.InFunction() and state.IsFunctionClose():
        is_immediately_called = (token.next and
                                 token.next.type == Type.START_PAREN)

        function = state.GetFunction()
        if not self._limited_doc_checks:
          if (function.has_return and function.doc and
              not is_immediately_called and
              not function.doc.HasFlag('return') and
              not function.doc.InheritsDocumentation() and
              not function.doc.HasFlag('constructor')):
            # Check for proper documentation of return value.
            self._HandleError(
                errors.MISSING_RETURN_DOCUMENTATION,
                'Missing @return JsDoc in function with non-trivial return',
                function.doc.end_token, Position.AtBeginning())
          elif (not function.has_return and function.doc and
                function.doc.HasFlag('return') and
                not state.InInterfaceMethod()):
            return_flag = function.doc.GetFlag('return')
            if (return_flag.type is None or (
                'undefined' not in return_flag.type and
                'void' not in return_flag.type and
                '*' not in return_flag.type)):
              self._HandleError(
                  errors.UNNECESSARY_RETURN_DOCUMENTATION,
                  'Found @return JsDoc on function that returns nothing',
                  return_flag.flag_token, Position.AtBeginning())

      if state.InFunction() and state.IsFunctionClose():
        is_immediately_called = (token.next and
                                 token.next.type == Type.START_PAREN)
        if (function.has_this and function.doc and
            not function.doc.HasFlag('this') and
            not function.is_constructor and
            not function.is_interface and
            '.prototype.' not in function.name):
          self._HandleError(
              errors.MISSING_JSDOC_TAG_THIS,
              'Missing @this JsDoc in function referencing "this". ('
              'this usually means you are trying to reference "this" in '
              'a static function, or you have forgotten to mark a '
              'constructor with @constructor)',
              function.doc.end_token, Position.AtBeginning())

    elif type == Type.IDENTIFIER:
      if token.string == 'goog.inherits' and not state.InFunction():
        if state.GetLastNonSpaceToken().line_number == token.line_number:
          self._HandleError(
              errors.MISSING_LINE,
              'Missing newline between constructor and goog.inherits',
              token,
              Position.AtBeginning())

        extra_space = state.GetLastNonSpaceToken().next
        while extra_space != token:
          if extra_space.type == Type.BLANK_LINE:
            self._HandleError(
                errors.EXTRA_LINE,
                'Extra line between constructor and goog.inherits',
                extra_space)
          extra_space = extra_space.next

        # TODO(robbyw): Test the last function was a constructor.
        # TODO(robbyw): Test correct @extends and @implements documentation.

    elif type == Type.OPERATOR:
      # If the token is unary and appears to be used in a unary context
      # it's ok.  Otherwise, if it's at the end of the line or immediately
      # before a comment, it's ok.
      # Don't report an error before a start bracket - it will be reported
      # by that token's space checks.
      if (not token.metadata.IsUnaryOperator() and not last_in_line
          and not token.next.IsComment()
          and not token.next.IsOperator(',')
          and not token.next.type in (Type.WHITESPACE, Type.END_PAREN,
                                      Type.END_BRACKET, Type.SEMICOLON,
                                      Type.START_BRACKET)):
        self._HandleError(
            errors.MISSING_SPACE,
            'Missing space after "%s"' % token.string,
            token,
            Position.AtEnd(token.string))
    elif type == Type.WHITESPACE:
      # Check whitespace length if it's not the first token of the line and
      # if it's not immediately before a comment.
      if not last_in_line and not first_in_line and not token.next.IsComment():
        # Ensure there is no space after opening parentheses.
        if (token.previous.type in (Type.START_PAREN, Type.START_BRACKET,
                                    Type.FUNCTION_NAME)
            or token.next.type == Type.START_PARAMETERS):
          self._HandleError(
              errors.EXTRA_SPACE,
              'Extra space after "%s"' % token.previous.string,
              token,
              Position.All(token.string))

  def Finalize(self, state, tokenizer_mode):
    """Perform all checks that need to occur after all lines are processed."""
    # Call the base class's Finalize function.
    super(JavaScriptLintRules, self).Finalize(state, tokenizer_mode)

    # Check for sorted requires statements.
    goog_require_tokens = state.GetGoogRequireTokens()
    requires = [require_token.string for require_token in goog_require_tokens]
    sorted_requires = sorted(requires)
    index = 0
    bad = False
    for item in requires:
      if item != sorted_requires[index]:
        bad = True
        break
      index += 1

    if bad:
      self._HandleError(
          errors.GOOG_REQUIRES_NOT_ALPHABETIZED,
          'goog.require classes must be alphabetized.  The correct code is:\n' +
          '\n'.join(map(lambda x: 'goog.require(\'%s\');' % x,
                        sorted_requires)),
          goog_require_tokens[index],
          position=Position.AtBeginning(),
          fix_data=goog_require_tokens)

    # Check for sorted provides statements.
    goog_provide_tokens = state.GetGoogProvideTokens()
    provides = [provide_token.string for provide_token in goog_provide_tokens]
    sorted_provides = sorted(provides)
    index = 0
    bad = False
    for item in provides:
      if item != sorted_provides[index]:
        bad = True
        break
      index += 1

    if bad:
      self._HandleError(
          errors.GOOG_PROVIDES_NOT_ALPHABETIZED,
          'goog.provide classes must be alphabetized.  The correct code is:\n' +
          '\n'.join(map(lambda x: 'goog.provide(\'%s\');' % x,
                        sorted_provides)),
          goog_provide_tokens[index],
          position=Position.AtBeginning(),
          fix_data=goog_provide_tokens)

    if FLAGS.closurized_namespaces:
      # Check that we provide everything we need.
      provided_namespaces = state.GetProvidedNamespaces()
      missing_provides = provided_namespaces - set(provides)
      if missing_provides:
        self._HandleError(
            errors.MISSING_GOOG_PROVIDE,
            'Missing the following goog.provide statements:\n' +
            '\n'.join(map(lambda x: 'goog.provide(\'%s\');' % x,
                          sorted(missing_provides))),
            state.GetFirstToken(), position=Position.AtBeginning(),
            fix_data=missing_provides)

      # Compose a set of all available namespaces. Explicitly omit goog
      # because if you can call goog.require, you already have goog.
      available_namespaces = (set(requires) | set(provides) | set(['goog']) |
                              provided_namespaces)

      # Check that we require everything we need.
      missing_requires = set()
      for namespace_variants in state.GetUsedNamespaces():
        # Namespace variants is a list of potential things to require. If we
        # find we're missing one, we are lazy and choose to require the first
        # in the sequence - which should be the namespace.
        if not set(namespace_variants) & available_namespaces:
          missing_requires.add(namespace_variants[0])

      if missing_requires:
        self._HandleError(
            errors.MISSING_GOOG_REQUIRE,
            'Missing the following goog.require statements:\n' +
            '\n'.join(map(lambda x: 'goog.require(\'%s\');' % x,
                          sorted(missing_requires))),
            state.GetFirstToken(), position=Position.AtBeginning(),
            fix_data=missing_requires)

      # Check that we don't require things we don't actually use.
      namespace_variants = state.GetUsedNamespaces()
      used_namespaces = set()
      for a, b in namespace_variants:
        used_namespaces.add(a)
        used_namespaces.add(b)

      extra_requires = set()
      for i in requires:
        baseNamespace = i.split('.')[0]
        if (i not in used_namespaces and
            baseNamespace in FLAGS.closurized_namespaces and
            i not in FLAGS.ignored_extra_namespaces):
          extra_requires.add(i)

      if extra_requires:
        self._HandleError(
            errors.EXTRA_GOOG_REQUIRE,
            'The following goog.require statements appear unnecessary:\n' +
            '\n'.join(map(lambda x: 'goog.require(\'%s\');' % x,
                          sorted(extra_requires))),
            state.GetFirstToken(), position=Position.AtBeginning(),
            fix_data=extra_requires)

