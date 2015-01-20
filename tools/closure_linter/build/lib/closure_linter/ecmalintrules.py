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

"""Core methods for checking EcmaScript files for common style guide violations.
"""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)',
              'jacobr@google.com (Jacob Richman)')

import re

import gflags as flags

from closure_linter import checkerbase
from closure_linter import ecmametadatapass
from closure_linter import error_check
from closure_linter import errorrules
from closure_linter import errors
from closure_linter import indentation
from closure_linter import javascripttokenizer
from closure_linter import javascripttokens
from closure_linter import statetracker
from closure_linter import tokenutil
from closure_linter.common import error
from closure_linter.common import position


FLAGS = flags.FLAGS
flags.DEFINE_list('custom_jsdoc_tags', '', 'Extra jsdoc tags to allow')
# TODO(user): When flipping this to True, remove logic from unit tests
# that overrides this flag.
flags.DEFINE_boolean('dot_on_next_line', False, 'Require dots to be'
                     'placed on the next line for wrapped expressions')

# TODO(robbyw): Check for extra parens on return statements
# TODO(robbyw): Check for 0px in strings
# TODO(robbyw): Ensure inline jsDoc is in {}
# TODO(robbyw): Check for valid JS types in parameter docs

# Shorthand
Context = ecmametadatapass.EcmaContext
Error = error.Error
Modes = javascripttokenizer.JavaScriptModes
Position = position.Position
Rule = error_check.Rule
Type = javascripttokens.JavaScriptTokenType


class EcmaScriptLintRules(checkerbase.LintRulesBase):
  """EmcaScript lint style checking rules.

  Can be used to find common style errors in JavaScript, ActionScript and other
  Ecma like scripting languages.  Style checkers for Ecma scripting languages
  should inherit from this style checker.
  Please do not add any state to EcmaScriptLintRules or to any subclasses.

  All state should be added to the StateTracker subclass used for a particular
  language.
  """

  # It will be initialized in constructor so the flags are initialized.
  max_line_length = -1

  # Static constants.
  MISSING_PARAMETER_SPACE = re.compile(r',\S')

  EXTRA_SPACE = re.compile(r'(\(\s|\s\))')

  ENDS_WITH_SPACE = re.compile(r'\s$')

  ILLEGAL_TAB = re.compile(r'\t')

  # Regex used to split up complex types to check for invalid use of ? and |.
  TYPE_SPLIT = re.compile(r'[,<>()]')

  # Regex for form of author lines after the @author tag.
  AUTHOR_SPEC = re.compile(r'(\s*)[^\s]+@[^(\s]+(\s*)\(.+\)')

  # Acceptable tokens to remove for line too long testing.
  LONG_LINE_IGNORE = frozenset(
      ['*', '//', '@see'] +
      ['@%s' % tag for tag in statetracker.DocFlag.HAS_TYPE])

  JSDOC_FLAGS_DESCRIPTION_NOT_REQUIRED = frozenset([
      '@fileoverview', '@param', '@return', '@returns'])

  def __init__(self):
    """Initialize this lint rule object."""
    checkerbase.LintRulesBase.__init__(self)
    if EcmaScriptLintRules.max_line_length == -1:
      EcmaScriptLintRules.max_line_length = errorrules.GetMaxLineLength()

  def Initialize(self, checker, limited_doc_checks, is_html):
    """Initialize this lint rule object before parsing a new file."""
    checkerbase.LintRulesBase.Initialize(self, checker, limited_doc_checks,
                                         is_html)
    self._indentation = indentation.IndentationRules()

  def HandleMissingParameterDoc(self, token, param_name):
    """Handle errors associated with a parameter missing a @param tag."""
    raise TypeError('Abstract method HandleMissingParameterDoc not implemented')

  def _CheckLineLength(self, last_token, state):
    """Checks whether the line is too long.

    Args:
      last_token: The last token in the line.
      state: parser_state object that indicates the current state in the page
    """
    # Start from the last token so that we have the flag object attached to
    # and DOC_FLAG tokens.
    line_number = last_token.line_number
    token = last_token

    # Build a representation of the string where spaces indicate potential
    # line-break locations.
    line = []
    while token and token.line_number == line_number:
      if state.IsTypeToken(token):
        line.insert(0, 'x' * len(token.string))
      elif token.type in (Type.IDENTIFIER, Type.OPERATOR):
        # Dots are acceptable places to wrap (may be tokenized as identifiers).
        line.insert(0, token.string.replace('.', ' '))
      else:
        line.insert(0, token.string)
      token = token.previous

    line = ''.join(line)
    line = line.rstrip('\n\r\f')
    try:
      length = len(unicode(line, 'utf-8'))
    except (LookupError, UnicodeDecodeError):
      # Unknown encoding. The line length may be wrong, as was originally the
      # case for utf-8 (see bug 1735846). For now just accept the default
      # length, but as we find problems we can either add test for other
      # possible encodings or return without an error to protect against
      # false positives at the cost of more false negatives.
      length = len(line)

    if length > EcmaScriptLintRules.max_line_length:

      # If the line matches one of the exceptions, then it's ok.
      for long_line_regexp in self.GetLongLineExceptions():
        if long_line_regexp.match(last_token.line):
          return

      # If the line consists of only one "word", or multiple words but all
      # except one are ignoreable, then it's ok.
      parts = set(line.split())

      # We allow two "words" (type and name) when the line contains @param
      max_parts = 1
      if '@param' in parts:
        max_parts = 2

      # Custom tags like @requires may have url like descriptions, so ignore
      # the tag, similar to how we handle @see.
      custom_tags = set(['@%s' % f for f in FLAGS.custom_jsdoc_tags])
      if (len(parts.difference(self.LONG_LINE_IGNORE | custom_tags))
          > max_parts):
        self._HandleError(
            errors.LINE_TOO_LONG,
            'Line too long (%d characters).' % len(line), last_token)

  def _CheckJsDocType(self, token, js_type):
    """Checks the given type for style errors.

    Args:
      token: The DOC_FLAG token for the flag whose type to check.
      js_type: The flag's typeannotation.TypeAnnotation instance.
    """
    if not js_type: return

    if js_type.type_group and len(js_type.sub_types) == 2:
      identifiers = [t.identifier for t in js_type.sub_types]
      if 'null' in identifiers:
        # Don't warn if the identifier is a template type (e.g. {TYPE|null}.
        if not identifiers[0].isupper() and not identifiers[1].isupper():
          self._HandleError(
              errors.JSDOC_PREFER_QUESTION_TO_PIPE_NULL,
              'Prefer "?Type" to "Type|null": "%s"' % js_type, token)

    # TODO(user): We should report an error for wrong usage of '?' and '|'
    # e.g. {?number|string|null} etc.

    for sub_type in js_type.IterTypes():
      self._CheckJsDocType(token, sub_type)

  def _CheckForMissingSpaceBeforeToken(self, token):
    """Checks for a missing space at the beginning of a token.

    Reports a MISSING_SPACE error if the token does not begin with a space or
    the previous token doesn't end with a space and the previous token is on the
    same line as the token.

    Args:
      token: The token being checked
    """
    # TODO(user): Check if too many spaces?
    if (len(token.string) == len(token.string.lstrip()) and
        token.previous and token.line_number == token.previous.line_number and
        len(token.previous.string) - len(token.previous.string.rstrip()) == 0):
      self._HandleError(
          errors.MISSING_SPACE,
          'Missing space before "%s"' % token.string,
          token,
          position=Position.AtBeginning())

  def _CheckOperator(self, token):
    """Checks an operator for spacing and line style.

    Args:
      token: The operator token.
    """
    last_code = token.metadata.last_code

    if not self._ExpectSpaceBeforeOperator(token):
      if (token.previous and token.previous.type == Type.WHITESPACE and
          last_code and last_code.type in (Type.NORMAL, Type.IDENTIFIER) and
          last_code.line_number == token.line_number):
        self._HandleError(
            errors.EXTRA_SPACE, 'Extra space before "%s"' % token.string,
            token.previous, position=Position.All(token.previous.string))

    elif (token.previous and
          not token.previous.IsComment() and
          not tokenutil.IsDot(token) and
          token.previous.type in Type.EXPRESSION_ENDER_TYPES):
      self._HandleError(errors.MISSING_SPACE,
                        'Missing space before "%s"' % token.string, token,
                        position=Position.AtBeginning())

    # Check wrapping of operators.
    next_code = tokenutil.GetNextCodeToken(token)

    is_dot = tokenutil.IsDot(token)
    wrapped_before = last_code and last_code.line_number != token.line_number
    wrapped_after = next_code and next_code.line_number != token.line_number

    if FLAGS.dot_on_next_line and is_dot and wrapped_after:
      self._HandleError(
          errors.LINE_ENDS_WITH_DOT,
          '"." must go on the following line',
          token)
    if (not is_dot and wrapped_before and
        not token.metadata.IsUnaryOperator()):
      self._HandleError(
          errors.LINE_STARTS_WITH_OPERATOR,
          'Binary operator must go on previous line "%s"' % token.string,
          token)

  def _IsLabel(self, token):
    # A ':' token is considered part of a label if it occurs in a case
    # statement, a plain label, or an object literal, i.e. is not part of a
    # ternary.

    return (token.string == ':' and
            token.metadata.context.type in (Context.LITERAL_ELEMENT,
                                            Context.CASE_BLOCK,
                                            Context.STATEMENT))

  def _ExpectSpaceBeforeOperator(self, token):
    """Returns whether a space should appear before the given operator token.

    Args:
      token: The operator token.

    Returns:
      Whether there should be a space before the token.
    """
    if token.string == ',' or token.metadata.IsUnaryPostOperator():
      return False

    if tokenutil.IsDot(token):
      return False

    # Colons should appear in labels, object literals, the case of a switch
    # statement, and ternary operator. Only want a space in the case of the
    # ternary operator.
    if self._IsLabel(token):
      return False

    if token.metadata.IsUnaryOperator() and token.IsFirstInLine():
      return False

    return True

  def CheckToken(self, token, state):
    """Checks a token, given the current parser_state, for warnings and errors.

    Args:
      token: The current token under consideration
      state: parser_state object that indicates the current state in the page
    """
    # Store some convenience variables
    first_in_line = token.IsFirstInLine()
    last_in_line = token.IsLastInLine()
    last_non_space_token = state.GetLastNonSpaceToken()

    token_type = token.type

    # Process the line change.
    if not self._is_html and error_check.ShouldCheck(Rule.INDENTATION):
      # TODO(robbyw): Support checking indentation in HTML files.
      indentation_errors = self._indentation.CheckToken(token, state)
      for indentation_error in indentation_errors:
        self._HandleError(*indentation_error)

    if last_in_line:
      self._CheckLineLength(token, state)

    if token_type == Type.PARAMETERS:
      # Find missing spaces in parameter lists.
      if self.MISSING_PARAMETER_SPACE.search(token.string):
        fix_data = ', '.join([s.strip() for s in token.string.split(',')])
        self._HandleError(errors.MISSING_SPACE, 'Missing space after ","',
                          token, position=None, fix_data=fix_data.strip())

      # Find extra spaces at the beginning of parameter lists.  Make sure
      # we aren't at the beginning of a continuing multi-line list.
      if not first_in_line:
        space_count = len(token.string) - len(token.string.lstrip())
        if space_count:
          self._HandleError(errors.EXTRA_SPACE, 'Extra space after "("',
                            token, position=Position(0, space_count))

    elif (token_type == Type.START_BLOCK and
          token.metadata.context.type == Context.BLOCK):
      self._CheckForMissingSpaceBeforeToken(token)

    elif token_type == Type.END_BLOCK:
      last_code = token.metadata.last_code
      if state.InFunction() and state.IsFunctionClose():
        if state.InTopLevelFunction():
          # A semicolons should not be included at the end of a function
          # declaration.
          if not state.InAssignedFunction():
            if not last_in_line and token.next.type == Type.SEMICOLON:
              self._HandleError(
                  errors.ILLEGAL_SEMICOLON_AFTER_FUNCTION,
                  'Illegal semicolon after function declaration',
                  token.next, position=Position.All(token.next.string))

        # A semicolon should be included at the end of a function expression
        # that is not immediately called or used by a dot operator.
        if (state.InAssignedFunction() and token.next
            and token.next.type != Type.SEMICOLON):
          next_token = tokenutil.GetNextCodeToken(token)
          is_immediately_used = (next_token.type == Type.START_PAREN or
                                 tokenutil.IsDot(next_token))
          if not is_immediately_used:
            self._HandleError(
                errors.MISSING_SEMICOLON_AFTER_FUNCTION,
                'Missing semicolon after function assigned to a variable',
                token, position=Position.AtEnd(token.string))

        if state.InInterfaceMethod() and last_code.type != Type.START_BLOCK:
          self._HandleError(errors.INTERFACE_METHOD_CANNOT_HAVE_CODE,
                            'Interface methods cannot contain code', last_code)

      elif (state.IsBlockClose() and
            token.next and token.next.type == Type.SEMICOLON):
        if (last_code.metadata.context.parent.type != Context.OBJECT_LITERAL
            and last_code.metadata.context.type != Context.OBJECT_LITERAL):
          self._HandleError(
              errors.REDUNDANT_SEMICOLON,
              'No semicolon is required to end a code block',
              token.next, position=Position.All(token.next.string))

    elif token_type == Type.SEMICOLON:
      if token.previous and token.previous.type == Type.WHITESPACE:
        self._HandleError(
            errors.EXTRA_SPACE, 'Extra space before ";"',
            token.previous, position=Position.All(token.previous.string))

      if token.next and token.next.line_number == token.line_number:
        if token.metadata.context.type != Context.FOR_GROUP_BLOCK:
          # TODO(robbyw): Error about no multi-statement lines.
          pass

        elif token.next.type not in (
            Type.WHITESPACE, Type.SEMICOLON, Type.END_PAREN):
          self._HandleError(
              errors.MISSING_SPACE,
              'Missing space after ";" in for statement',
              token.next,
              position=Position.AtBeginning())

      last_code = token.metadata.last_code
      if last_code and last_code.type == Type.SEMICOLON:
        # Allow a single double semi colon in for loops for cases like:
        # for (;;) { }.
        # NOTE(user): This is not a perfect check, and will not throw an error
        # for cases like: for (var i = 0;; i < n; i++) {}, but then your code
        # probably won't work either.
        for_token = tokenutil.CustomSearch(
            last_code,
            lambda token: token.type == Type.KEYWORD and token.string == 'for',
            end_func=lambda token: token.type == Type.SEMICOLON,
            distance=None,
            reverse=True)

        if not for_token:
          self._HandleError(errors.REDUNDANT_SEMICOLON, 'Redundant semicolon',
                            token, position=Position.All(token.string))

    elif token_type == Type.START_PAREN:
      # Ensure that opening parentheses have a space before any keyword
      # that is not being invoked like a member function.
      if (token.previous and token.previous.type == Type.KEYWORD and
          (not token.previous.metadata or
           not token.previous.metadata.last_code or
           not token.previous.metadata.last_code.string or
           token.previous.metadata.last_code.string[-1:] != '.')):
        self._HandleError(errors.MISSING_SPACE, 'Missing space before "("',
                          token, position=Position.AtBeginning())
      elif token.previous and token.previous.type == Type.WHITESPACE:
        before_space = token.previous.previous
        # Ensure that there is no extra space before a function invocation,
        # even if the function being invoked happens to be a keyword.
        if (before_space and before_space.line_number == token.line_number and
            before_space.type == Type.IDENTIFIER or
            (before_space.type == Type.KEYWORD and before_space.metadata and
             before_space.metadata.last_code and
             before_space.metadata.last_code.string and
             before_space.metadata.last_code.string[-1:] == '.')):
          self._HandleError(
              errors.EXTRA_SPACE, 'Extra space before "("',
              token.previous, position=Position.All(token.previous.string))

    elif token_type == Type.START_BRACKET:
      self._HandleStartBracket(token, last_non_space_token)
    elif token_type in (Type.END_PAREN, Type.END_BRACKET):
      # Ensure there is no space before closing parentheses, except when
      # it's in a for statement with an omitted section, or when it's at the
      # beginning of a line.
      if (token.previous and token.previous.type == Type.WHITESPACE and
          not token.previous.IsFirstInLine() and
          not (last_non_space_token and last_non_space_token.line_number ==
               token.line_number and
               last_non_space_token.type == Type.SEMICOLON)):
        self._HandleError(
            errors.EXTRA_SPACE, 'Extra space before "%s"' %
            token.string, token.previous,
            position=Position.All(token.previous.string))

    elif token_type == Type.WHITESPACE:
      if self.ILLEGAL_TAB.search(token.string):
        if token.IsFirstInLine():
          if token.next:
            self._HandleError(
                errors.ILLEGAL_TAB,
                'Illegal tab in whitespace before "%s"' % token.next.string,
                token, position=Position.All(token.string))
          else:
            self._HandleError(
                errors.ILLEGAL_TAB,
                'Illegal tab in whitespace',
                token, position=Position.All(token.string))
        else:
          self._HandleError(
              errors.ILLEGAL_TAB,
              'Illegal tab in whitespace after "%s"' % token.previous.string,
              token, position=Position.All(token.string))

      # Check whitespace length if it's not the first token of the line and
      # if it's not immediately before a comment.
      if last_in_line:
        # Check for extra whitespace at the end of a line.
        self._HandleError(errors.EXTRA_SPACE, 'Extra space at end of line',
                          token, position=Position.All(token.string))
      elif not first_in_line and not token.next.IsComment():
        if token.length > 1:
          self._HandleError(
              errors.EXTRA_SPACE, 'Extra space after "%s"' %
              token.previous.string, token,
              position=Position(1, len(token.string) - 1))

    elif token_type == Type.OPERATOR:
      self._CheckOperator(token)
    elif token_type == Type.DOC_FLAG:
      flag = token.attached_object

      if flag.flag_type == 'bug':
        # TODO(robbyw): Check for exactly 1 space on the left.
        string = token.next.string.lstrip()
        string = string.split(' ', 1)[0]

        if not string.isdigit():
          self._HandleError(errors.NO_BUG_NUMBER_AFTER_BUG_TAG,
                            '@bug should be followed by a bug number', token)

      elif flag.flag_type == 'suppress':
        if flag.type is None:
          # A syntactically invalid suppress tag will get tokenized as a normal
          # flag, indicating an error.
          self._HandleError(
              errors.INCORRECT_SUPPRESS_SYNTAX,
              'Invalid suppress syntax: should be @suppress {errortype}. '
              'Spaces matter.', token)
        else:
          for suppress_type in flag.jstype.IterIdentifiers():
            if suppress_type not in state.GetDocFlag().SUPPRESS_TYPES:
              self._HandleError(
                  errors.INVALID_SUPPRESS_TYPE,
                  'Invalid suppression type: %s' % suppress_type, token)

      elif (error_check.ShouldCheck(Rule.WELL_FORMED_AUTHOR) and
            flag.flag_type == 'author'):
        # TODO(user): In non strict mode check the author tag for as much as
        # it exists, though the full form checked below isn't required.
        string = token.next.string
        result = self.AUTHOR_SPEC.match(string)
        if not result:
          self._HandleError(errors.INVALID_AUTHOR_TAG_DESCRIPTION,
                            'Author tag line should be of the form: '
                            '@author foo@somewhere.com (Your Name)',
                            token.next)
        else:
          # Check spacing between email address and name. Do this before
          # checking earlier spacing so positions are easier to calculate for
          # autofixing.
          num_spaces = len(result.group(2))
          if num_spaces < 1:
            self._HandleError(errors.MISSING_SPACE,
                              'Missing space after email address',
                              token.next, position=Position(result.start(2), 0))
          elif num_spaces > 1:
            self._HandleError(
                errors.EXTRA_SPACE, 'Extra space after email address',
                token.next,
                position=Position(result.start(2) + 1, num_spaces - 1))

          # Check for extra spaces before email address. Can't be too few, if
          # not at least one we wouldn't match @author tag.
          num_spaces = len(result.group(1))
          if num_spaces > 1:
            self._HandleError(errors.EXTRA_SPACE,
                              'Extra space before email address',
                              token.next, position=Position(1, num_spaces - 1))

      elif (flag.flag_type in state.GetDocFlag().HAS_DESCRIPTION and
            not self._limited_doc_checks):
        if flag.flag_type == 'param':
          if flag.name is None:
            self._HandleError(errors.MISSING_JSDOC_PARAM_NAME,
                              'Missing name in @param tag', token)

        if not flag.description or flag.description is None:
          flag_name = token.type
          if 'name' in token.values:
            flag_name = '@' + token.values['name']

          if flag_name not in self.JSDOC_FLAGS_DESCRIPTION_NOT_REQUIRED:
            self._HandleError(
                errors.MISSING_JSDOC_TAG_DESCRIPTION,
                'Missing description in %s tag' % flag_name, token)
        else:
          self._CheckForMissingSpaceBeforeToken(flag.description_start_token)

      if flag.HasType():
        if flag.type_start_token is not None:
          self._CheckForMissingSpaceBeforeToken(
              token.attached_object.type_start_token)

        if flag.jstype and not flag.jstype.IsEmpty():
          self._CheckJsDocType(token, flag.jstype)

          if error_check.ShouldCheck(Rule.BRACES_AROUND_TYPE) and (
              flag.type_start_token.type != Type.DOC_START_BRACE or
              flag.type_end_token.type != Type.DOC_END_BRACE):
            self._HandleError(
                errors.MISSING_BRACES_AROUND_TYPE,
                'Type must always be surrounded by curly braces.', token)

    if token_type in (Type.DOC_FLAG, Type.DOC_INLINE_FLAG):
      if (token.values['name'] not in state.GetDocFlag().LEGAL_DOC and
          token.values['name'] not in FLAGS.custom_jsdoc_tags):
        self._HandleError(
            errors.INVALID_JSDOC_TAG,
            'Invalid JsDoc tag: %s' % token.values['name'], token)

      if (error_check.ShouldCheck(Rule.NO_BRACES_AROUND_INHERIT_DOC) and
          token.values['name'] == 'inheritDoc' and
          token_type == Type.DOC_INLINE_FLAG):
        self._HandleError(errors.UNNECESSARY_BRACES_AROUND_INHERIT_DOC,
                          'Unnecessary braces around @inheritDoc',
                          token)

    elif token_type == Type.SIMPLE_LVALUE:
      identifier = token.values['identifier']

      if ((not state.InFunction() or state.InConstructor()) and
          state.InTopLevel() and not state.InObjectLiteralDescendant()):
        jsdoc = state.GetDocComment()
        if not state.HasDocComment(identifier):
          # Only test for documentation on identifiers with .s in them to
          # avoid checking things like simple variables. We don't require
          # documenting assignments to .prototype itself (bug 1880803).
          if (not state.InConstructor() and
              identifier.find('.') != -1 and not
              identifier.endswith('.prototype') and not
              self._limited_doc_checks):
            comment = state.GetLastComment()
            if not (comment and comment.lower().count('jsdoc inherited')):
              self._HandleError(
                  errors.MISSING_MEMBER_DOCUMENTATION,
                  "No docs found for member '%s'" % identifier,
                  token)
        elif jsdoc and (not state.InConstructor() or
                        identifier.startswith('this.')):
          # We are at the top level and the function/member is documented.
          if identifier.endswith('_') and not identifier.endswith('__'):
            # Can have a private class which inherits documentation from a
            # public superclass.
            #
            # @inheritDoc is deprecated in favor of using @override, and they
            if (jsdoc.HasFlag('override') and not jsdoc.HasFlag('constructor')
                and ('accessControls' not in jsdoc.suppressions)):
              self._HandleError(
                  errors.INVALID_OVERRIDE_PRIVATE,
                  '%s should not override a private member.' % identifier,
                  jsdoc.GetFlag('override').flag_token)
            if (jsdoc.HasFlag('inheritDoc') and not jsdoc.HasFlag('constructor')
                and ('accessControls' not in jsdoc.suppressions)):
              self._HandleError(
                  errors.INVALID_INHERIT_DOC_PRIVATE,
                  '%s should not inherit from a private member.' % identifier,
                  jsdoc.GetFlag('inheritDoc').flag_token)
            if (not jsdoc.HasFlag('private') and
                ('underscore' not in jsdoc.suppressions) and not
                ((jsdoc.HasFlag('inheritDoc') or jsdoc.HasFlag('override')) and
                 ('accessControls' in jsdoc.suppressions))):
              self._HandleError(
                  errors.MISSING_PRIVATE,
                  'Member "%s" must have @private JsDoc.' %
                  identifier, token)
            if jsdoc.HasFlag('private') and 'underscore' in jsdoc.suppressions:
              self._HandleError(
                  errors.UNNECESSARY_SUPPRESS,
                  '@suppress {underscore} is not necessary with @private',
                  jsdoc.suppressions['underscore'])
          elif (jsdoc.HasFlag('private') and
                not self.InExplicitlyTypedLanguage()):
            # It is convention to hide public fields in some ECMA
            # implementations from documentation using the @private tag.
            self._HandleError(
                errors.EXTRA_PRIVATE,
                'Member "%s" must not have @private JsDoc' %
                identifier, token)

          # These flags are only legal on localizable message definitions;
          # such variables always begin with the prefix MSG_.
          for f in ('desc', 'hidden', 'meaning'):
            if (jsdoc.HasFlag(f)
                and not identifier.startswith('MSG_')
                and identifier.find('.MSG_') == -1):
              self._HandleError(
                  errors.INVALID_USE_OF_DESC_TAG,
                  'Member "%s" should not have @%s JsDoc' % (identifier, f),
                  token)

      # Check for illegaly assigning live objects as prototype property values.
      index = identifier.find('.prototype.')
      # Ignore anything with additional .s after the prototype.
      if index != -1 and identifier.find('.', index + 11) == -1:
        equal_operator = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES)
        next_code = tokenutil.SearchExcept(equal_operator, Type.NON_CODE_TYPES)
        if next_code and (
            next_code.type in (Type.START_BRACKET, Type.START_BLOCK) or
            next_code.IsOperator('new')):
          self._HandleError(
              errors.ILLEGAL_PROTOTYPE_MEMBER_VALUE,
              'Member %s cannot have a non-primitive value' % identifier,
              token)

    elif token_type == Type.END_PARAMETERS:
      # Find extra space at the end of parameter lists.  We check the token
      # prior to the current one when it is a closing paren.
      if (token.previous and token.previous.type == Type.PARAMETERS
          and self.ENDS_WITH_SPACE.search(token.previous.string)):
        self._HandleError(errors.EXTRA_SPACE, 'Extra space before ")"',
                          token.previous)

      jsdoc = state.GetDocComment()
      if state.GetFunction().is_interface:
        if token.previous and token.previous.type == Type.PARAMETERS:
          self._HandleError(
              errors.INTERFACE_CONSTRUCTOR_CANNOT_HAVE_PARAMS,
              'Interface constructor cannot have parameters',
              token.previous)
      elif (state.InTopLevel() and jsdoc and not jsdoc.HasFlag('see')
            and not jsdoc.InheritsDocumentation()
            and not state.InObjectLiteralDescendant() and not
            jsdoc.IsInvalidated()):
        distance, edit = jsdoc.CompareParameters(state.GetParams())
        if distance:
          params_iter = iter(state.GetParams())
          docs_iter = iter(jsdoc.ordered_params)

          for op in edit:
            if op == 'I':
              # Insertion.
              # Parsing doc comments is the same for all languages
              # but some languages care about parameters that don't have
              # doc comments and some languages don't care.
              # Languages that don't allow variables to by typed such as
              # JavaScript care but languages such as ActionScript or Java
              # that allow variables to be typed don't care.
              if not self._limited_doc_checks:
                self.HandleMissingParameterDoc(token, params_iter.next())

            elif op == 'D':
              # Deletion
              self._HandleError(errors.EXTRA_PARAMETER_DOCUMENTATION,
                                'Found docs for non-existing parameter: "%s"' %
                                docs_iter.next(), token)
            elif op == 'S':
              # Substitution
              if not self._limited_doc_checks:
                self._HandleError(
                    errors.WRONG_PARAMETER_DOCUMENTATION,
                    'Parameter mismatch: got "%s", expected "%s"' %
                    (params_iter.next(), docs_iter.next()), token)

            else:
              # Equality - just advance the iterators
              params_iter.next()
              docs_iter.next()

    elif token_type == Type.STRING_TEXT:
      # If this is the first token after the start of the string, but it's at
      # the end of a line, we know we have a multi-line string.
      if token.previous.type in (
          Type.SINGLE_QUOTE_STRING_START,
          Type.DOUBLE_QUOTE_STRING_START) and last_in_line:
        self._HandleError(errors.MULTI_LINE_STRING,
                          'Multi-line strings are not allowed', token)

    # This check is orthogonal to the ones above, and repeats some types, so
    # it is a plain if and not an elif.
    if token.type in Type.COMMENT_TYPES:
      if self.ILLEGAL_TAB.search(token.string):
        self._HandleError(errors.ILLEGAL_TAB,
                          'Illegal tab in comment "%s"' % token.string, token)

      trimmed = token.string.rstrip()
      if last_in_line and token.string != trimmed:
        # Check for extra whitespace at the end of a line.
        self._HandleError(
            errors.EXTRA_SPACE, 'Extra space at end of line', token,
            position=Position(len(trimmed), len(token.string) - len(trimmed)))

    # This check is also orthogonal since it is based on metadata.
    if token.metadata.is_implied_semicolon:
      self._HandleError(errors.MISSING_SEMICOLON,
                        'Missing semicolon at end of line', token)

  def _HandleStartBracket(self, token, last_non_space_token):
    """Handles a token that is an open bracket.

    Args:
      token: The token to handle.
      last_non_space_token: The last token that was not a space.
    """
    if (not token.IsFirstInLine() and token.previous.type == Type.WHITESPACE and
        last_non_space_token and
        last_non_space_token.type in Type.EXPRESSION_ENDER_TYPES):
      self._HandleError(
          errors.EXTRA_SPACE, 'Extra space before "["',
          token.previous, position=Position.All(token.previous.string))
    # If the [ token is the first token in a line we shouldn't complain
    # about a missing space before [.  This is because some Ecma script
    # languages allow syntax like:
    # [Annotation]
    # class MyClass {...}
    # So we don't want to blindly warn about missing spaces before [.
    # In the the future, when rules for computing exactly how many spaces
    # lines should be indented are added, then we can return errors for
    # [ tokens that are improperly indented.
    # For example:
    # var someVeryVeryVeryVeryVeryVeryVeryVeryVeryVeryVeryLongVariableName =
    # [a,b,c];
    # should trigger a proper indentation warning message as [ is not indented
    # by four spaces.
    elif (not token.IsFirstInLine() and token.previous and
          token.previous.type not in (
              [Type.WHITESPACE, Type.START_PAREN, Type.START_BRACKET] +
              Type.EXPRESSION_ENDER_TYPES)):
      self._HandleError(errors.MISSING_SPACE, 'Missing space before "["',
                        token, position=Position.AtBeginning())

  def Finalize(self, state):
    """Perform all checks that need to occur after all lines are processed.

    Args:
      state: State of the parser after parsing all tokens

    Raises:
      TypeError: If not overridden.
    """
    last_non_space_token = state.GetLastNonSpaceToken()
    # Check last line for ending with newline.
    if state.GetLastLine() and not (
        state.GetLastLine().isspace() or
        state.GetLastLine().rstrip('\n\r\f') != state.GetLastLine()):
      self._HandleError(
          errors.FILE_MISSING_NEWLINE,
          'File does not end with new line.  (%s)' % state.GetLastLine(),
          last_non_space_token)

    try:
      self._indentation.Finalize()
    except Exception, e:
      self._HandleError(
          errors.FILE_DOES_NOT_PARSE,
          str(e),
          last_non_space_token)

  def GetLongLineExceptions(self):
    """Gets a list of regexps for lines which can be longer than the limit.

    Returns:
      A list of regexps, used as matches (rather than searches).
    """
    return []

  def InExplicitlyTypedLanguage(self):
    """Returns whether this ecma implementation is explicitly typed."""
    return False
