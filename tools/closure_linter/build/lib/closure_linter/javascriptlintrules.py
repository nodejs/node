#!/usr/bin/env python
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
    self._declared_private_members = set()
    self._used_private_members = set()
    # A stack of dictionaries, one for each function scope entered. Each
    # dictionary is keyed by an identifier that defines a local variable and has
    # a token as its value.
    self._unused_local_variables_by_scope = []

  def HandleMissingParameterDoc(self, token, param_name):
    """Handle errors associated with a parameter missing a param tag."""
    self._HandleError(errors.MISSING_PARAMETER_DOCUMENTATION,
                      'Missing docs for parameter: "%s"' % param_name, token)

  # pylint: disable=too-many-statements
  def CheckToken(self, token, state):
    """Checks a token, given the current parser_state, for warnings and errors.

    Args:
      token: The current token under consideration
      state: parser_state object that indicates the current state in the page
    """

    # Call the base class's CheckToken function.
    super(JavaScriptLintRules, self).CheckToken(token, state)

    # Store some convenience variables
    namespaces_info = self._namespaces_info

    if error_check.ShouldCheck(Rule.UNUSED_LOCAL_VARIABLES):
      self._CheckUnusedLocalVariables(token, state)

    if error_check.ShouldCheck(Rule.UNUSED_PRIVATE_MEMBERS):
      # Find all assignments to private members.
      if token.type == Type.SIMPLE_LVALUE:
        identifier = token.string
        if identifier.endswith('_') and not identifier.endswith('__'):
          doc_comment = state.GetDocComment()
          suppressed = doc_comment and (
              'underscore' in doc_comment.suppressions or
              'unusedPrivateMembers' in doc_comment.suppressions)
          if not suppressed:
            # Look for static members defined on a provided namespace.
            if namespaces_info:
              namespace = namespaces_info.GetClosurizedNamespace(identifier)
              provided_namespaces = namespaces_info.GetProvidedNamespaces()
            else:
              namespace = None
              provided_namespaces = set()

            # Skip cases of this.something_.somethingElse_.
            regex = re.compile(r'^this\.[a-zA-Z_]+$')
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

        if flag.type is not None and flag.name is not None:
          if error_check.ShouldCheck(Rule.VARIABLE_ARG_MARKER):
            # Check for variable arguments marker in type.
            if flag.jstype.IsVarArgsType() and flag.name != 'var_args':
              self._HandleError(errors.JSDOC_MISSING_VAR_ARGS_NAME,
                                'Variable length argument %s must be renamed '
                                'to var_args.' % flag.name,
                                token)
            elif not flag.jstype.IsVarArgsType() and flag.name == 'var_args':
              self._HandleError(errors.JSDOC_MISSING_VAR_ARGS_TYPE,
                                'Variable length argument %s type must start '
                                'with \'...\'.' % flag.name,
                                token)

          if error_check.ShouldCheck(Rule.OPTIONAL_TYPE_MARKER):
            # Check for optional marker in type.
            if (flag.jstype.opt_arg and
                not flag.name.startswith('opt_')):
              self._HandleError(errors.JSDOC_MISSING_OPTIONAL_PREFIX,
                                'Optional parameter name %s must be prefixed '
                                'with opt_.' % flag.name,
                                token)
            elif (not flag.jstype.opt_arg and
                  flag.name.startswith('opt_')):
              self._HandleError(errors.JSDOC_MISSING_OPTIONAL_TYPE,
                                'Optional parameter %s type must end with =.' %
                                flag.name,
                                token)

      if flag.flag_type in state.GetDocFlag().HAS_TYPE:
        # Check for both missing type token and empty type braces '{}'
        # Missing suppress types are reported separately and we allow enums,
        # const, private, public and protected without types.
        if (flag.flag_type not in state.GetDocFlag().CAN_OMIT_TYPE
            and (not flag.jstype or flag.jstype.IsEmpty())):
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
            position=Position.All(token.string))

    elif token.type == Type.END_DOC_COMMENT:
      doc_comment = state.GetDocComment()

      # When @externs appears in a @fileoverview comment, it should trigger
      # the same limited doc checks as a special filename like externs.js.
      if doc_comment.HasFlag('fileoverview') and doc_comment.HasFlag('externs'):
        self._SetLimitedDocChecks(True)

      if (error_check.ShouldCheck(Rule.BLANK_LINES_AT_TOP_LEVEL) and
          not self._is_html and
          state.InTopLevel() and
          not state.InNonScopeBlock()):

        # Check if we're in a fileoverview or constructor JsDoc.
        is_constructor = (
            doc_comment.HasFlag('constructor') or
            doc_comment.HasFlag('interface'))
        # @fileoverview is an optional tag so if the dosctring is the first
        # token in the file treat it as a file level docstring.
        is_file_level_comment = (
            doc_comment.HasFlag('fileoverview') or
            not doc_comment.start_token.previous)

        # If the comment is not a file overview, and it does not immediately
        # precede some code, skip it.
        # NOTE: The tokenutil methods are not used here because of their
        # behavior at the top of a file.
        next_token = token.next
        if (not next_token or
            (not is_file_level_comment and
             next_token.type in Type.NON_CODE_TYPES)):
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
        if not is_file_level_comment:
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

        # Only need blank line before file overview if it is not the beginning
        # of the file, e.g. copyright is first.
        if is_file_level_comment and blank_lines == 0 and block_start.previous:
          error_message = 'Should have a blank line before a file overview.'
          expected_blank_lines = 1
        elif is_constructor and blank_lines != 3:
          error_message = (
              'Should have 3 blank lines before a constructor/interface.')
          expected_blank_lines = 3
        elif (not is_file_level_comment and not is_constructor and
              blank_lines != 2):
          error_message = 'Should have 2 blank lines between top-level blocks.'
          expected_blank_lines = 2

        if error_message:
          self._HandleError(
              errors.WRONG_BLANK_LINE_COUNT, error_message,
              block_start, position=Position.AtBeginning(),
              fix_data=expected_blank_lines - blank_lines)

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
                function.doc.end_token, position=Position.AtBeginning())
          elif (not function.has_return and
                not function.has_throw and
                function.doc and
                function.doc.HasFlag('return') and
                not state.InInterfaceMethod()):
            flag = function.doc.GetFlag('return')
            valid_no_return_names = ['undefined', 'void', '*']
            invalid_return = flag.jstype is None or not any(
                sub_type.identifier in valid_no_return_names
                for sub_type in flag.jstype.IterTypeGroup())

            if invalid_return:
              self._HandleError(
                  errors.UNNECESSARY_RETURN_DOCUMENTATION,
                  'Found @return JsDoc on function that returns nothing',
                  flag.flag_token, position=Position.AtBeginning())

        # b/4073735. Method in object literal definition of prototype can
        # safely reference 'this'.
        prototype_object_literal = False
        block_start = None
        previous_code = None
        previous_previous_code = None

        # Search for cases where prototype is defined as object literal.
        #       previous_previous_code
        #       |       previous_code
        #       |       | block_start
        #       |       | |
        # a.b.prototype = {
        #   c : function() {
        #     this.d = 1;
        #   }
        # }

        # If in object literal, find first token of block so to find previous
        # tokens to check above condition.
        if state.InObjectLiteral():
          block_start = state.GetCurrentBlockStart()

        # If an object literal then get previous token (code type). For above
        # case it should be '='.
        if block_start:
          previous_code = tokenutil.SearchExcept(block_start,
                                                 Type.NON_CODE_TYPES,
                                                 reverse=True)

        # If previous token to block is '=' then get its previous token.
        if previous_code and previous_code.IsOperator('='):
          previous_previous_code = tokenutil.SearchExcept(previous_code,
                                                          Type.NON_CODE_TYPES,
                                                          reverse=True)

        # If variable/token before '=' ends with '.prototype' then its above
        # case of prototype defined with object literal.
        prototype_object_literal = (previous_previous_code and
                                    previous_previous_code.string.endswith(
                                        '.prototype'))

        if (function.has_this and function.doc and
            not function.doc.HasFlag('this') and
            not function.is_constructor and
            not function.is_interface and
            '.prototype.' not in function.name and
            not prototype_object_literal):
          self._HandleError(
              errors.MISSING_JSDOC_TAG_THIS,
              'Missing @this JsDoc in function referencing "this". ('
              'this usually means you are trying to reference "this" in '
              'a static function, or you have forgotten to mark a '
              'constructor with @constructor)',
              function.doc.end_token, position=Position.AtBeginning())

    elif token.type == Type.IDENTIFIER:
      if token.string == 'goog.inherits' and not state.InFunction():
        if state.GetLastNonSpaceToken().line_number == token.line_number:
          self._HandleError(
              errors.MISSING_LINE,
              'Missing newline between constructor and goog.inherits',
              token,
              position=Position.AtBeginning())

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
        namespace = tokenutil.GetStringAfterToken(token)

        # Report extra goog.provide statement.
        if not namespace or namespaces_info.IsExtraProvide(token):
          if not namespace:
            msg = 'Empty namespace in goog.provide'
          else:
            msg = 'Unnecessary goog.provide: ' +  namespace

            # Hint to user if this is a Test namespace.
            if namespace.endswith('Test'):
              msg += (' *Test namespaces must be mentioned in the '
                      'goog.setTestOnly() call')

          self._HandleError(
              errors.EXTRA_GOOG_PROVIDE,
              msg,
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
            missing_requires, illegal_alias_statements = (
                namespaces_info.GetMissingRequires())
            if missing_requires:
              self._ReportMissingRequires(
                  missing_requires,
                  tokenutil.GetLastTokenInSameLine(token).next,
                  True)
            if illegal_alias_statements:
              self._ReportIllegalAliasStatement(illegal_alias_statements)

      elif (token.string == 'goog.require' and
            not state.InFunction() and
            namespaces_info is not None):
        namespace = tokenutil.GetStringAfterToken(token)

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
        if not namespace or namespaces_info.IsExtraRequire(token):
          if not namespace:
            msg = 'Empty namespace in goog.require'
          else:
            msg = 'Unnecessary goog.require: ' + namespace

          self._HandleError(
              errors.EXTRA_GOOG_REQUIRE,
              msg,
              token, position=Position.AtBeginning())

        # Report missing goog.require statements.
        if namespaces_info.IsLastRequire(token):
          missing_requires, illegal_alias_statements = (
              namespaces_info.GetMissingRequires())
          if missing_requires:
            self._ReportMissingRequires(
                missing_requires,
                tokenutil.GetLastTokenInSameLine(token).next,
                False)
          if illegal_alias_statements:
            self._ReportIllegalAliasStatement(illegal_alias_statements)

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
          and not tokenutil.IsDot(token)
          and token.next.type not in (Type.WHITESPACE, Type.END_PAREN,
                                      Type.END_BRACKET, Type.SEMICOLON,
                                      Type.START_BRACKET)):
        self._HandleError(
            errors.MISSING_SPACE,
            'Missing space after "%s"' % token.string,
            token,
            position=Position.AtEnd(token.string))
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
              position=Position.All(token.string))
    elif token.type == Type.SEMICOLON:
      previous_token = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES,
                                              reverse=True)
      if not previous_token:
        self._HandleError(
            errors.REDUNDANT_SEMICOLON,
            'Semicolon without any statement',
            token,
            position=Position.AtEnd(token.string))
      elif (previous_token.type == Type.KEYWORD and
            previous_token.string not in ['break', 'continue', 'return']):
        self._HandleError(
            errors.REDUNDANT_SEMICOLON,
            ('Semicolon after \'%s\' without any statement.'
             ' Looks like an error.' % previous_token.string),
            token,
            position=Position.AtEnd(token.string))

  def _CheckUnusedLocalVariables(self, token, state):
    """Checks for unused local variables in function blocks.

    Args:
      token: The token to check.
      state: The state tracker.
    """
    # We don't use state.InFunction because that disregards scope functions.
    in_function = state.FunctionDepth() > 0
    if token.type == Type.SIMPLE_LVALUE or token.type == Type.IDENTIFIER:
      if in_function:
        identifier = token.string
        # Check whether the previous token was var.
        previous_code_token = tokenutil.CustomSearch(
            token,
            lambda t: t.type not in Type.NON_CODE_TYPES,
            reverse=True)
        if previous_code_token and previous_code_token.IsKeyword('var'):
          # Add local variable declaration to the top of the unused locals
          # stack.
          self._unused_local_variables_by_scope[-1][identifier] = token
        elif token.type == Type.IDENTIFIER:
          # This covers most cases where the variable is used as an identifier.
          self._MarkLocalVariableUsed(token.string)
        elif token.type == Type.SIMPLE_LVALUE and '.' in identifier:
          # This covers cases where a value is assigned to a property of the
          # variable.
          self._MarkLocalVariableUsed(token.string)
    elif token.type == Type.START_BLOCK:
      if in_function and state.IsFunctionOpen():
        # Push a new map onto the stack
        self._unused_local_variables_by_scope.append({})
    elif token.type == Type.END_BLOCK:
      if state.IsFunctionClose():
        # Pop the stack and report any remaining locals as unused.
        unused_local_variables = self._unused_local_variables_by_scope.pop()
        for unused_token in unused_local_variables.values():
          self._HandleError(
              errors.UNUSED_LOCAL_VARIABLE,
              'Unused local variable: %s.' % unused_token.string,
              unused_token)
    elif token.type == Type.DOC_FLAG:
      # Flags that use aliased symbols should be counted.
      flag = token.attached_object
      js_type = flag and flag.jstype
      if flag and flag.flag_type in state.GetDocFlag().HAS_TYPE and js_type:
        self._MarkAliasUsed(js_type)

  def _MarkAliasUsed(self, js_type):
    """Marks aliases in a type as used.

    Recursively iterates over all subtypes in a jsdoc type annotation and
    tracks usage of aliased symbols (which may be local variables).
    Marks the local variable as used in the scope nearest to the current
    scope that matches the given token.

    Args:
      js_type: The jsdoc type, a typeannotation.TypeAnnotation object.
    """
    if js_type.alias:
      self._MarkLocalVariableUsed(js_type.identifier)
    for sub_type in js_type.IterTypes():
      self._MarkAliasUsed(sub_type)

  def _MarkLocalVariableUsed(self, identifier):
    """Marks the local variable as used in the relevant scope.

    Marks the local variable in the scope nearest to the current scope that
    matches the given identifier as used.

    Args:
      identifier: The identifier representing the potential usage of a local
                  variable.
    """
    identifier = identifier.split('.', 1)[0]
    # Find the first instance of the identifier in the stack of function scopes
    # and mark it used.
    for unused_local_variables in reversed(
        self._unused_local_variables_by_scope):
      if identifier in unused_local_variables:
        del unused_local_variables[identifier]
        break

  def _ReportMissingProvides(self, missing_provides, token, need_blank_line):
    """Reports missing provide statements to the error handler.

    Args:
      missing_provides: A dictionary of string(key) and integer(value) where
          each string(key) is a namespace that should be provided, but is not
          and integer(value) is first line number where it's required.
      token: The token where the error was detected (also where the new provides
          will be inserted.
      need_blank_line: Whether a blank line needs to be inserted after the new
          provides are inserted. May be True, False, or None, where None
          indicates that the insert location is unknown.
    """

    missing_provides_msg = 'Missing the following goog.provide statements:\n'
    missing_provides_msg += '\n'.join(['goog.provide(\'%s\');' % x for x in
                                       sorted(missing_provides)])
    missing_provides_msg += '\n'

    missing_provides_msg += '\nFirst line where provided: \n'
    missing_provides_msg += '\n'.join(
        ['  %s : line %d' % (x, missing_provides[x]) for x in
         sorted(missing_provides)])
    missing_provides_msg += '\n'

    self._HandleError(
        errors.MISSING_GOOG_PROVIDE,
        missing_provides_msg,
        token, position=Position.AtBeginning(),
        fix_data=(missing_provides.keys(), need_blank_line))

  def _ReportMissingRequires(self, missing_requires, token, need_blank_line):
    """Reports missing require statements to the error handler.

    Args:
      missing_requires: A dictionary of string(key) and integer(value) where
          each string(key) is a namespace that should be required, but is not
          and integer(value) is first line number where it's required.
      token: The token where the error was detected (also where the new requires
          will be inserted.
      need_blank_line: Whether a blank line needs to be inserted before the new
          requires are inserted. May be True, False, or None, where None
          indicates that the insert location is unknown.
    """

    missing_requires_msg = 'Missing the following goog.require statements:\n'
    missing_requires_msg += '\n'.join(['goog.require(\'%s\');' % x for x in
                                       sorted(missing_requires)])
    missing_requires_msg += '\n'

    missing_requires_msg += '\nFirst line where required: \n'
    missing_requires_msg += '\n'.join(
        ['  %s : line %d' % (x, missing_requires[x]) for x in
         sorted(missing_requires)])
    missing_requires_msg += '\n'

    self._HandleError(
        errors.MISSING_GOOG_REQUIRE,
        missing_requires_msg,
        token, position=Position.AtBeginning(),
        fix_data=(missing_requires.keys(), need_blank_line))

  def _ReportIllegalAliasStatement(self, illegal_alias_statements):
    """Reports alias statements that would need a goog.require."""
    for namespace, token in illegal_alias_statements.iteritems():
      self._HandleError(
          errors.ALIAS_STMT_NEEDS_GOOG_REQUIRE,
          'The alias definition would need the namespace \'%s\' which is not '
          'required through any other symbol.' % namespace,
          token, position=Position.AtBeginning())

  def Finalize(self, state):
    """Perform all checks that need to occur after all lines are processed."""
    # Call the base class's Finalize function.
    super(JavaScriptLintRules, self).Finalize(state)

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
      self._declared_private_members = set()
      self._used_private_members = set()

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

        missing_requires, illegal_alias = namespaces_info.GetMissingRequires()
        if missing_requires:
          self._ReportMissingRequires(
              missing_requires, state.GetFirstToken(), None)
        if illegal_alias:
          self._ReportIllegalAliasStatement(illegal_alias)

    self._CheckSortedRequiresProvides(state.GetFirstToken())

  def _CheckSortedRequiresProvides(self, token):
    """Checks that all goog.require and goog.provide statements are sorted.

    Note that this method needs to be run after missing statements are added to
    preserve alphabetical order.

    Args:
      token: The first token in the token stream.
    """
    sorter = requireprovidesorter.RequireProvideSorter()
    first_provide_token = sorter.CheckProvides(token)
    if first_provide_token:
      new_order = sorter.GetFixedProvideString(first_provide_token)
      self._HandleError(
          errors.GOOG_PROVIDES_NOT_ALPHABETIZED,
          'goog.provide classes must be alphabetized.  The correct code is:\n' +
          new_order,
          first_provide_token,
          position=Position.AtBeginning(),
          fix_data=first_provide_token)

    first_require_token = sorter.CheckRequires(token)
    if first_require_token:
      new_order = sorter.GetFixedRequireString(first_require_token)
      self._HandleError(
          errors.GOOG_REQUIRES_NOT_ALPHABETIZED,
          'goog.require classes must be alphabetized.  The correct code is:\n' +
          new_order,
          first_require_token,
          position=Position.AtBeginning(),
          fix_data=first_require_token)

  def GetLongLineExceptions(self):
    """Gets a list of regexps for lines which can be longer than the limit.

    Returns:
      A list of regexps, used as matches (rather than searches).
    """
    return [
        re.compile(r'(var .+\s*=\s*)?goog\.require\(.+\);?\s*$'),
        re.compile(r'goog\.(provide|module|setTestOnly)\(.+\);?\s*$'),
        re.compile(r'[\s/*]*@visibility\s*{.*}[\s*/]*$'),
        ]
