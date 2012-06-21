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

"""Methods for checking JS files for common style guide violations.

These style guide violations should only apply to JavaScript and not an Ecma
scripting languages.
"""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)',
              'jacobr@google.com (Jacob Richman)')

import re
from sets import Set
from closure_linter import ecmalintrules
from closure_linter import error_check
from closure_linter import errors
from closure_linter import javascripttokenizer
from closure_linter import javascripttokens
from closure_linter import requireprovidesorter
from closure_linter import tokenutil
from closure_linter.common import error
from closure_linter.common import position

# Shorthand
Error = error.Error
Position = position.Position
Rule = error_check.Rule
Type = javascripttokens.JavaScriptTokenType


class JavaScriptLintRules(ecmalintrules.EcmaScriptLintRules):
  """JavaScript lint rules that catch JavaScript specific style errors."""

  def __init__(self, namespaces_info):
    """Initializes a JavaScriptLintRules instance."""
    ecmalintrules.EcmaScriptLintRules.__init__(self)
    self._namespaces_info = namespaces_info
    self._declared_private_member_tokens = {}
    self._declared_private_members = Set()
    self._used_private_members = Set()

  def HandleMissingParameterDoc(self, token, param_name):
    """Handle errors associated with a parameter missing a param tag."""
    self._HandleError(errors.MISSING_PARAMETER_DOCUMENTATION,
                      'Missing docs for parameter: "%s"' % param_name, token)

  def __ContainsRecordType(self, token):
    """Check whether the given token contains a record type.

    Args:
      token: The token being checked

    Returns:
      True if the token contains a record type, False otherwise.
    """
    # If we see more than one left-brace in the string of an annotation token,
    # then there's a record type in there.
    return (
        token and token.type == Type.DOC_FLAG and
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
    namespaces_info = self._namespaces_info

    if error_check.ShouldCheck(Rule.UNUSED_PRIVATE_MEMBERS):
      # Find all assignments to private members.
      if token.type == Type.SIMPLE_LVALUE:
        identifier = token.string
        if identifier.endswith('_') and not identifier.endswith('__'):
          doc_comment = state.GetDocComment()
          suppressed = (doc_comment and doc_comment.HasFlag('suppress') and
                        doc_comment.GetFlag('suppress').type == 'underscore')
          if not suppressed:
            # Look for static members defined on a provided namespace.
            namespace = namespaces_info.GetClosurizedNamespace(identifier)
            provided_namespaces = namespaces_info.GetProvidedNamespaces()

            # Skip cases of this.something_.somethingElse_.
            regex = re.compile('^this\.[a-zA-Z_]+$')
            if namespace in provided_namespaces or regex.match(identifier):
              variable = identifier.split('.')[-1]
              self._declared_private_member_tokens[variable] = token
              self._declared_private_members.add(variable)
        elif not identifier.endswith('__'):
          # Consider setting public members of private members to be a usage.
          for piece in identifier.split('.'):
            if piece.endswith('_'):
              self._used_private_members.add(piece)

      # Find all usages of private members.
      if token.type == Type.IDENTIFIER:
        for piece in token.string.split('.'):
          if piece.endswith('_'):
            self._used_private_members.add(piece)

    if token.type == Type.DOC_FLAG:
      flag = token.attached_object

      if flag.flag_type == 'param' and flag.name_token is not None:
        self._CheckForMissingSpaceBeforeToken(
            token.attached_object.name_token)

        if (error_check.ShouldCheck(Rule.OPTIONAL_TYPE_MARKER) and
            flag.type is not None and flag.name is not None):
          # Check for optional marker in type.
          if (flag.type.endswith('=') and
              not flag.name.startswith('opt_')):
            self._HandleError(errors.JSDOC_MISSING_OPTIONAL_PREFIX,
                              'Optional parameter name %s must be prefixed '
                              'with opt_.' % flag.name,
                              token)
          elif (not flag.type.endswith('=') and
                flag.name.startswith('opt_')):
            self._HandleError(errors.JSDOC_MISSING_OPTIONAL_TYPE,
                              'Optional parameter %s type must end with =.' %
                              flag.name,
                              token)

      if flag.flag_type in state.GetDocFlag().HAS_TYPE:
        # Check for both missing type token and empty type braces '{}'
        # Missing suppress types are reported separately and we allow enums
        # without types.
        if (flag.flag_type not in ('suppress', 'enum') and
            (not flag.type or flag.type.isspace())):
          self._HandleError(errors.MISSING_JSDOC_TAG_TYPE,
                            'Missing type in %s tag' % token.string, token)

        elif flag.name_token and flag.type_end_token and tokenutil.Compare(
            flag.type_end_token, flag.name_token) > 0:
          self._HandleError(
              errors.OUT_OF_ORDER_JSDOC_TAG_TYPE,
              'Type should be immediately after %s tag' % token.string,
              token)

    elif token.type == Type.DOUBLE_QUOTE_STRING_START:
      next_token = token.next
      while next_token.type == Type.STRING_TEXT:
        if javascripttokenizer.JavaScriptTokenizer.SINGLE_QUOTE.search(
            next_token.string):
          break
        next_token = next_token.next
      else:
        self._HandleError(
            errors.UNNECESSARY_DOUBLE_QUOTED_STRING,
            'Single-quoted string preferred over double-quoted string.',
            token,
            Position.All(token.string))

    elif token.type == Type.END_DOC_COMMENT:
      doc_comment = state.GetDocComment()

      # When @externs appears in a @fileoverview comment, it should trigger
      # the same limited doc checks as a special filename like externs.js.
      if doc_comment.HasFlag('fileoverview') and doc_comment.HasFlag('externs'):
        self._SetLimitedDocChecks(True)

      if (error_check.ShouldCheck(Rule.BLANK_LINES_AT_TOP_LEVEL) and
          not self._is_html and state.InTopLevel() and not state.InBlock()):

        # Check if we're in a fileoverview or constructor JsDoc.
        is_constructor = (
            doc_comment.HasFlag('constructor') or
            doc_comment.HasFlag('interface'))
        is_file_overview = doc_comment.HasFlag('fileoverview')

        # If the comment is not a file overview, and it does not immediately
        # precede some code, skip it.
        # NOTE: The tokenutil methods are not used here because of their
        # behavior at the top of a file.
        next_token = token.next
        if (not next_token or
            (not is_file_overview and next_token.type in Type.NON_CODE_TYPES)):
          return

        # Don't require extra blank lines around suppression of extra
        # goog.require errors.
        if (doc_comment.SuppressionOnly() and
            next_token.type == Type.IDENTIFIER and
            next_token.string in ['goog.provide', 'goog.require']):
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
          error_message = (
              'Should have 3 blank lines before a constructor/interface.')
          expected_blank_lines = 3
        elif not is_file_overview and not is_constructor and blank_lines != 2:
          error_message = 'Should have 2 blank lines between top-level blocks.'
          expected_blank_lines = 2

        if error_message:
          self._HandleError(
              errors.WRONG_BLANK_LINE_COUNT, error_message,
              block_start, Position.AtBeginning(),
              expected_blank_lines - blank_lines)

    elif token.type == Type.END_BLOCK:
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
          elif (not function.has_return and
                not function.has_throw and
                function.doc and
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

    elif token.type == Type.IDENTIFIER:
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

      elif (token.string == 'goog.provide' and
            not state.InFunction() and
            namespaces_info is not None):
        namespace = tokenutil.Search(token, Type.STRING_TEXT).string

        # Report extra goog.provide statement.
        if namespaces_info.IsExtraProvide(token):
          self._HandleError(
              errors.EXTRA_GOOG_PROVIDE,
              'Unnecessary goog.provide: ' + namespace,
              token, position=Position.AtBeginning())

        if namespaces_info.IsLastProvide(token):
          # Report missing provide statements after the last existing provide.
          missing_provides = namespaces_info.GetMissingProvides()
          if missing_provides:
            self._ReportMissingProvides(
                missing_provides,
                tokenutil.GetLastTokenInSameLine(token).next,
                False)

          # If there are no require statements, missing requires should be
          # reported after the last provide.
          if not namespaces_info.GetRequiredNamespaces():
            missing_requires = namespaces_info.GetMissingRequires()
            if missing_requires:
              self._ReportMissingRequires(
                  missing_requires,
                  tokenutil.GetLastTokenInSameLine(token).next,
                  True)

      elif (token.string == 'goog.require' and
            not state.InFunction() and
            namespaces_info is not None):
        namespace = tokenutil.Search(token, Type.STRING_TEXT).string

        # If there are no provide statements, missing provides should be
        # reported before the first require.
        if (namespaces_info.IsFirstRequire(token) and
            not namespaces_info.GetProvidedNamespaces()):
          missing_provides = namespaces_info.GetMissingProvides()
          if missing_provides:
            self._ReportMissingProvides(
                missing_provides,
                tokenutil.GetFirstTokenInSameLine(token),
                True)

        # Report extra goog.require statement.
        if namespaces_info.IsExtraRequire(token):
          self._HandleError(
              errors.EXTRA_GOOG_REQUIRE,
              'Unnecessary goog.require: ' + namespace,
              token, position=Position.AtBeginning())

        # Report missing goog.require statements.
        if namespaces_info.IsLastRequire(token):
          missing_requires = namespaces_info.GetMissingRequires()
          if missing_requires:
            self._ReportMissingRequires(
                missing_requires,
                tokenutil.GetLastTokenInSameLine(token).next,
                False)

    elif token.type == Type.OPERATOR:
      last_in_line = token.IsLastInLine()
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
    elif token.type == Type.WHITESPACE:
      first_in_line = token.IsFirstInLine()
      last_in_line = token.IsLastInLine()
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

  def _ReportMissingProvides(self, missing_provides, token, need_blank_line):
    """Reports missing provide statements to the error handler.

    Args:
      missing_provides: A list of strings where each string is a namespace that
          should be provided, but is not.
      token: The token where the error was detected (also where the new provides
          will be inserted.
      need_blank_line: Whether a blank line needs to be inserted after the new
          provides are inserted. May be True, False, or None, where None
          indicates that the insert location is unknown.
    """
    self._HandleError(
        errors.MISSING_GOOG_PROVIDE,
        'Missing the following goog.provide statements:\n' +
        '\n'.join(map(lambda x: 'goog.provide(\'%s\');' % x,
                      sorted(missing_provides))),
        token, position=Position.AtBeginning(),
        fix_data=(missing_provides, need_blank_line))

  def _ReportMissingRequires(self, missing_requires, token, need_blank_line):
    """Reports missing require statements to the error handler.

    Args:
      missing_requires: A list of strings where each string is a namespace that
          should be required, but is not.
      token: The token where the error was detected (also where the new requires
          will be inserted.
      need_blank_line: Whether a blank line needs to be inserted before the new
          requires are inserted. May be True, False, or None, where None
          indicates that the insert location is unknown.
    """
    self._HandleError(
        errors.MISSING_GOOG_REQUIRE,
        'Missing the following goog.require statements:\n' +
        '\n'.join(map(lambda x: 'goog.require(\'%s\');' % x,
                      sorted(missing_requires))),
        token, position=Position.AtBeginning(),
        fix_data=(missing_requires, need_blank_line))

  def Finalize(self, state, tokenizer_mode):
    """Perform all checks that need to occur after all lines are processed."""
    # Call the base class's Finalize function.
    super(JavaScriptLintRules, self).Finalize(state, tokenizer_mode)

    if error_check.ShouldCheck(Rule.UNUSED_PRIVATE_MEMBERS):
      # Report an error for any declared private member that was never used.
      unused_private_members = (self._declared_private_members -
                                self._used_private_members)

      for variable in unused_private_members:
        token = self._declared_private_member_tokens[variable]
        self._HandleError(errors.UNUSED_PRIVATE_MEMBER,
                          'Unused private member: %s.' % token.string,
                          token)

      # Clear state to prepare for the next file.
      self._declared_private_member_tokens = {}
      self._declared_private_members = Set()
      self._used_private_members = Set()

    namespaces_info = self._namespaces_info
    if namespaces_info is not None:
      # If there are no provide or require statements, missing provides and
      # requires should be reported on line 1.
      if (not namespaces_info.GetProvidedNamespaces() and
          not namespaces_info.GetRequiredNamespaces()):
        missing_provides = namespaces_info.GetMissingProvides()
        if missing_provides:
          self._ReportMissingProvides(
              missing_provides, state.GetFirstToken(), None)

        missing_requires = namespaces_info.GetMissingRequires()
        if missing_requires:
          self._ReportMissingRequires(
              missing_requires, state.GetFirstToken(), None)

    self._CheckSortedRequiresProvides(state.GetFirstToken())

  def _CheckSortedRequiresProvides(self, token):
    """Checks that all goog.require and goog.provide statements are sorted.

    Note that this method needs to be run after missing statements are added to
    preserve alphabetical order.

    Args:
      token: The first token in the token stream.
    """
    sorter = requireprovidesorter.RequireProvideSorter()
    provides_result = sorter.CheckProvides(token)
    if provides_result:
      self._HandleError(
          errors.GOOG_PROVIDES_NOT_ALPHABETIZED,
          'goog.provide classes must be alphabetized.  The correct code is:\n' +
          '\n'.join(
              map(lambda x: 'goog.provide(\'%s\');' % x, provides_result[1])),
          provides_result[0],
          position=Position.AtBeginning(),
          fix_data=provides_result[0])

    requires_result = sorter.CheckRequires(token)
    if requires_result:
      self._HandleError(
          errors.GOOG_REQUIRES_NOT_ALPHABETIZED,
          'goog.require classes must be alphabetized.  The correct code is:\n' +
          '\n'.join(
              map(lambda x: 'goog.require(\'%s\');' % x, requires_result[1])),
          requires_result[0],
          position=Position.AtBeginning(),
          fix_data=requires_result[0])

  def GetLongLineExceptions(self):
    """Gets a list of regexps for lines which can be longer than the limit."""
    return [
        re.compile('goog\.require\(.+\);?\s*$'),
        re.compile('goog\.provide\(.+\);?\s*$')
        ]
