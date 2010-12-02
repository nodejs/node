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

from closure_linter import checkerbase
from closure_linter import ecmametadatapass
from closure_linter import errors
from closure_linter import indentation
from closure_linter import javascripttokens
from closure_linter import javascripttokenizer
from closure_linter import statetracker
from closure_linter import tokenutil
from closure_linter.common import error
from closure_linter.common import htmlutil
from closure_linter.common import lintrunner
from closure_linter.common import position
from closure_linter.common import tokens
import gflags as flags

FLAGS = flags.FLAGS
flags.DEFINE_boolean('strict', False,
                     'Whether to validate against the stricter Closure style.')
flags.DEFINE_list('custom_jsdoc_tags', '', 'Extra jsdoc tags to allow')

# TODO(robbyw): Check for extra parens on return statements
# TODO(robbyw): Check for 0px in strings
# TODO(robbyw): Ensure inline jsDoc is in {}
# TODO(robbyw): Check for valid JS types in parameter docs

# Shorthand
Context = ecmametadatapass.EcmaContext
Error = error.Error
Modes = javascripttokenizer.JavaScriptModes
Position = position.Position
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

  # Static constants.
  MAX_LINE_LENGTH = 80

  MISSING_PARAMETER_SPACE = re.compile(r',\S')

  EXTRA_SPACE = re.compile('(\(\s|\s\))')

  ENDS_WITH_SPACE = re.compile('\s$')

  ILLEGAL_TAB = re.compile(r'\t')

  # Regex used to split up complex types to check for invalid use of ? and |.
  TYPE_SPLIT = re.compile(r'[,<>()]')

  # Regex for form of author lines after the @author tag.
  AUTHOR_SPEC = re.compile(r'(\s*)[^\s]+@[^(\s]+(\s*)\(.+\)')

  # Acceptable tokens to remove for line too long testing.
  LONG_LINE_IGNORE = frozenset(['*', '//', '@see'] +
      ['@%s' % tag for tag in statetracker.DocFlag.HAS_TYPE])

  def __init__(self):
    """Initialize this lint rule object."""
    checkerbase.LintRulesBase.__init__(self)

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
      elif token.type in (Type.IDENTIFIER, Type.NORMAL):
        # Dots are acceptable places to wrap.
        line.insert(0, token.string.replace('.', ' '))
      else:
        line.insert(0, token.string)
      token = token.previous

    line = ''.join(line)
    line = line.rstrip('\n\r\f')
    try:
      length = len(unicode(line, 'utf-8'))
    except:
      # Unknown encoding. The line length may be wrong, as was originally the
      # case for utf-8 (see bug 1735846). For now just accept the default
      # length, but as we find problems we can either add test for other
      # possible encodings or return without an error to protect against
      # false positives at the cost of more false negatives.
      length = len(line)

    if length > self.MAX_LINE_LENGTH:

      # If the line matches one of the exceptions, then it's ok.
      for long_line_regexp in self.GetLongLineExceptions():
        if long_line_regexp.match(last_token.line):
          return

      # If the line consists of only one "word", or multiple words but all
      # except one are ignoreable, then it's ok.
      parts = set(line.split())

      # We allow two "words" (type and name) when the line contains @param
      max = 1
      if '@param' in parts:
        max = 2

      # Custom tags like @requires may have url like descriptions, so ignore
      # the tag, similar to how we handle @see.
      custom_tags = set(['@%s' % f for f in FLAGS.custom_jsdoc_tags])
      if (len(parts.difference(self.LONG_LINE_IGNORE | custom_tags)) > max):
        self._HandleError(errors.LINE_TOO_LONG,
            'Line too long (%d characters).' % len(line), last_token)

  def _CheckJsDocType(self, token):
    """Checks the given type for style errors.

    Args:
      token: The DOC_FLAG token for the flag whose type to check.
    """
    flag = token.attached_object
    type = flag.type
    if type and type is not None and not type.isspace():
      pieces = self.TYPE_SPLIT.split(type)
      if len(pieces) == 1 and type.count('|') == 1 and (
           type.endswith('|null') or type.startswith('null|')):
         self._HandleError(errors.JSDOC_PREFER_QUESTION_TO_PIPE_NULL,
             'Prefer "?Type" to "Type|null": "%s"' % type, token)

      for p in pieces:
        if p.count('|') and p.count('?'):
          # TODO(robbyw): We should do actual parsing of JsDoc types.  As is,
          # this won't report an error for {number|Array.<string>?}, etc.
          self._HandleError(errors.JSDOC_ILLEGAL_QUESTION_WITH_PIPE,
              'JsDoc types cannot contain both "?" and "|": "%s"' % p, token)

      if FLAGS.strict and (flag.type_start_token.type != Type.DOC_START_BRACE or
                           flag.type_end_token.type != Type.DOC_END_BRACE):
        self._HandleError(errors.MISSING_BRACES_AROUND_TYPE,
            'Type must always be surrounded by curly braces.', token)

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
          Position.AtBeginning())

  def _ExpectSpaceBeforeOperator(self, token):
    """Returns whether a space should appear before the given operator token.

    Args:
      token: The operator token.

    Returns:
      Whether there should be a space before the token.
    """
    if token.string == ',' or token.metadata.IsUnaryPostOperator():
      return False

    # Colons should appear in labels, object literals, the case of a switch
    # statement, and ternary operator. Only want a space in the case of the
    # ternary operator.
    if (token.string == ':' and
        token.metadata.context.type in (Context.LITERAL_ELEMENT,
                                        Context.CASE_BLOCK,
                                        Context.STATEMENT)):
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

    type = token.type

    # Process the line change.
    if not self._is_html and FLAGS.strict:
      # TODO(robbyw): Support checking indentation in HTML files.
      indentation_errors = self._indentation.CheckToken(token, state)
      for indentation_error in indentation_errors:
        self._HandleError(*indentation_error)

    if last_in_line:
      self._CheckLineLength(token, state)

    if type == Type.PARAMETERS:
      # Find missing spaces in parameter lists.
      if self.MISSING_PARAMETER_SPACE.search(token.string):
        self._HandleError(errors.MISSING_SPACE, 'Missing space after ","',
            token)

      # Find extra spaces at the beginning of parameter lists.  Make sure
      # we aren't at the beginning of a continuing multi-line list.
      if not first_in_line:
        space_count = len(token.string) - len(token.string.lstrip())
        if space_count:
          self._HandleError(errors.EXTRA_SPACE, 'Extra space after "("',
              token, Position(0, space_count))

    elif (type == Type.START_BLOCK and
          token.metadata.context.type == Context.BLOCK):
      self._CheckForMissingSpaceBeforeToken(token)

    elif type == Type.END_BLOCK:
      # This check is for object literal end block tokens, but there is no need
      # to test that condition since a comma at the end of any other kind of
      # block is undoubtedly a parse error.
      last_code = token.metadata.last_code
      if last_code.IsOperator(','):
        self._HandleError(errors.COMMA_AT_END_OF_LITERAL,
            'Illegal comma at end of object literal', last_code,
            Position.All(last_code.string))

      if state.InFunction() and state.IsFunctionClose():
        is_immediately_called = (token.next and
                                 token.next.type == Type.START_PAREN)
        if state.InTopLevelFunction():
          # When the function was top-level and not immediately called, check
          # that it's terminated by a semi-colon.
          if state.InAssignedFunction():
            if not is_immediately_called and (last_in_line or
                not token.next.type == Type.SEMICOLON):
              self._HandleError(errors.MISSING_SEMICOLON_AFTER_FUNCTION,
                  'Missing semicolon after function assigned to a variable',
                  token, Position.AtEnd(token.string))
          else:
            if not last_in_line and token.next.type == Type.SEMICOLON:
              self._HandleError(errors.ILLEGAL_SEMICOLON_AFTER_FUNCTION,
                  'Illegal semicolon after function declaration',
                  token.next, Position.All(token.next.string))

        if (state.InInterfaceMethod() and last_code.type != Type.START_BLOCK):
          self._HandleError(errors.INTERFACE_METHOD_CANNOT_HAVE_CODE,
              'Interface methods cannot contain code', last_code)

      elif (state.IsBlockClose() and
            token.next and token.next.type == Type.SEMICOLON):
        self._HandleError(errors.REDUNDANT_SEMICOLON,
            'No semicolon is required to end a code block',
            token.next, Position.All(token.next.string))

    elif type == Type.SEMICOLON:
      if token.previous and token.previous.type == Type.WHITESPACE:
        self._HandleError(errors.EXTRA_SPACE, 'Extra space before ";"',
            token.previous, Position.All(token.previous.string))

      if token.next and token.next.line_number == token.line_number:
        if token.metadata.context.type != Context.FOR_GROUP_BLOCK:
          # TODO(robbyw): Error about no multi-statement lines.
          pass

        elif token.next.type not in (
            Type.WHITESPACE, Type.SEMICOLON, Type.END_PAREN):
          self._HandleError(errors.MISSING_SPACE,
              'Missing space after ";" in for statement',
              token.next,
              Position.AtBeginning())

      last_code = token.metadata.last_code
      if last_code and last_code.type == Type.SEMICOLON:
        # Allow a single double semi colon in for loops for cases like:
        # for (;;) { }.
        # NOTE(user): This is not a perfect check, and will not throw an error
        # for cases like: for (var i = 0;; i < n; i++) {}, but then your code
        # probably won't work either.
        for_token = tokenutil.CustomSearch(last_code,
            lambda token: token.type == Type.KEYWORD and token.string == 'for',
            end_func=lambda token: token.type == Type.SEMICOLON,
            distance=None,
            reverse=True)

        if not for_token:
          self._HandleError(errors.REDUNDANT_SEMICOLON, 'Redundant semicolon',
              token, Position.All(token.string))

    elif type == Type.START_PAREN:
      if token.previous and token.previous.type == Type.KEYWORD:
        self._HandleError(errors.MISSING_SPACE, 'Missing space before "("',
            token, Position.AtBeginning())
      elif token.previous and token.previous.type == Type.WHITESPACE:
        before_space = token.previous.previous
        if (before_space and before_space.line_number == token.line_number and
            before_space.type == Type.IDENTIFIER):
          self._HandleError(errors.EXTRA_SPACE, 'Extra space before "("',
              token.previous, Position.All(token.previous.string))

    elif type == Type.START_BRACKET:
      if (not first_in_line and token.previous.type == Type.WHITESPACE and
          last_non_space_token and
          last_non_space_token.type in Type.EXPRESSION_ENDER_TYPES):
        self._HandleError(errors.EXTRA_SPACE, 'Extra space before "["',
            token.previous, Position.All(token.previous.string))
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
      elif (not first_in_line and token.previous and
            not token.previous.type in (
                [Type.WHITESPACE, Type.START_PAREN, Type.START_BRACKET] +
                Type.EXPRESSION_ENDER_TYPES)):
        self._HandleError(errors.MISSING_SPACE, 'Missing space before "["',
            token, Position.AtBeginning())

    elif type in (Type.END_PAREN, Type.END_BRACKET):
      # Ensure there is no space before closing parentheses, except when
      # it's in a for statement with an omitted section, or when it's at the
      # beginning of a line.
      if (token.previous and token.previous.type == Type.WHITESPACE and
          not token.previous.IsFirstInLine() and
          not (last_non_space_token and last_non_space_token.line_number ==
                   token.line_number and
               last_non_space_token.type == Type.SEMICOLON)):
        self._HandleError(errors.EXTRA_SPACE, 'Extra space before "%s"' %
            token.string, token.previous, Position.All(token.previous.string))

      if token.type == Type.END_BRACKET:
        last_code = token.metadata.last_code
        if last_code.IsOperator(','):
          self._HandleError(errors.COMMA_AT_END_OF_LITERAL,
              'Illegal comma at end of array literal', last_code,
              Position.All(last_code.string))

    elif type == Type.WHITESPACE:
      if self.ILLEGAL_TAB.search(token.string):
        if token.IsFirstInLine():
          self._HandleError(errors.ILLEGAL_TAB,
              'Illegal tab in whitespace before "%s"' % token.next.string,
              token, Position.All(token.string))
        else:
          self._HandleError(errors.ILLEGAL_TAB,
              'Illegal tab in whitespace after "%s"' % token.previous.string,
              token, Position.All(token.string))

      # Check whitespace length if it's not the first token of the line and
      # if it's not immediately before a comment.
      if last_in_line:
        # Check for extra whitespace at the end of a line.
        self._HandleError(errors.EXTRA_SPACE, 'Extra space at end of line',
            token, Position.All(token.string))
      elif not first_in_line and not token.next.IsComment():
        if token.length > 1:
          self._HandleError(errors.EXTRA_SPACE, 'Extra space after "%s"' %
              token.previous.string, token,
              Position(1, len(token.string) - 1))

    elif type == Type.OPERATOR:
      last_code = token.metadata.last_code

      if not self._ExpectSpaceBeforeOperator(token):
        if (token.previous and token.previous.type == Type.WHITESPACE and
            last_code and last_code.type in (Type.NORMAL, Type.IDENTIFIER)):
          self._HandleError(errors.EXTRA_SPACE,
              'Extra space before "%s"' % token.string, token.previous,
              Position.All(token.previous.string))

      elif (token.previous and
            not token.previous.IsComment() and
            token.previous.type in Type.EXPRESSION_ENDER_TYPES):
        self._HandleError(errors.MISSING_SPACE,
                          'Missing space before "%s"' % token.string, token,
                          Position.AtBeginning())

      # Check that binary operators are not used to start lines.
      if ((not last_code or last_code.line_number != token.line_number) and
          not token.metadata.IsUnaryOperator()):
        self._HandleError(errors.LINE_STARTS_WITH_OPERATOR,
            'Binary operator should go on previous line "%s"' % token.string,
            token)

    elif type == Type.DOC_FLAG:
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
          self._HandleError(errors.INCORRECT_SUPPRESS_SYNTAX,
              'Invalid suppress syntax: should be @suppress {errortype}. '
              'Spaces matter.', token)
        elif flag.type not in state.GetDocFlag().SUPPRESS_TYPES:
          self._HandleError(errors.INVALID_SUPPRESS_TYPE,
              'Invalid suppression type: %s' % flag.type,
              token)

      elif FLAGS.strict and flag.flag_type == 'author':
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
                              token.next, Position(result.start(2), 0))
          elif num_spaces > 1:
            self._HandleError(errors.EXTRA_SPACE,
                              'Extra space after email address',
                              token.next,
                              Position(result.start(2) + 1, num_spaces - 1))

          # Check for extra spaces before email address. Can't be too few, if
          # not at least one we wouldn't match @author tag.
          num_spaces = len(result.group(1))
          if num_spaces > 1:
            self._HandleError(errors.EXTRA_SPACE,
                              'Extra space before email address',
                              token.next, Position(1, num_spaces - 1))

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
          self._HandleError(errors.MISSING_JSDOC_TAG_DESCRIPTION,
              'Missing description in %s tag' % flag_name, token)
        else:
          self._CheckForMissingSpaceBeforeToken(flag.description_start_token)

          # We want punctuation to be inside of any tags ending a description,
          # so strip tags before checking description. See bug 1127192. Note
          # that depending on how lines break, the real description end token
          # may consist only of stripped html and the effective end token can
          # be different.
          end_token = flag.description_end_token
          end_string = htmlutil.StripTags(end_token.string).strip()
          while (end_string == '' and not
                 end_token.type in Type.FLAG_ENDING_TYPES):
            end_token = end_token.previous
            if end_token.type in Type.FLAG_DESCRIPTION_TYPES:
              end_string = htmlutil.StripTags(end_token.string).rstrip()

          if not (end_string.endswith('.') or end_string.endswith('?') or
              end_string.endswith('!')):
            # Find the position for the missing punctuation, inside of any html
            # tags.
            desc_str = end_token.string.rstrip()
            while desc_str.endswith('>'):
              start_tag_index = desc_str.rfind('<')
              if start_tag_index < 0:
                break              
              desc_str = desc_str[:start_tag_index].rstrip()
            end_position = Position(len(desc_str), 0)

            self._HandleError(
                errors.JSDOC_TAG_DESCRIPTION_ENDS_WITH_INVALID_CHARACTER,
                ('%s descriptions must end with valid punctuation such as a '
                 'period.' % token.string),
                end_token, end_position)

      if flag.flag_type in state.GetDocFlag().HAS_TYPE:
        if flag.type_start_token is not None:
          self._CheckForMissingSpaceBeforeToken(
              token.attached_object.type_start_token)

        if flag.type and flag.type != '' and not flag.type.isspace():
          self._CheckJsDocType(token)

    if type in (Type.DOC_FLAG, Type.DOC_INLINE_FLAG):
        if (token.values['name'] not in state.GetDocFlag().LEGAL_DOC and
            token.values['name'] not in FLAGS.custom_jsdoc_tags):
          self._HandleError(errors.INVALID_JSDOC_TAG,
              'Invalid JsDoc tag: %s' % token.values['name'], token)

        if (FLAGS.strict and token.values['name'] == 'inheritDoc' and
            type == Type.DOC_INLINE_FLAG):
          self._HandleError(errors.UNNECESSARY_BRACES_AROUND_INHERIT_DOC,
              'Unnecessary braces around @inheritDoc',
              token)

    elif type == Type.SIMPLE_LVALUE:
      identifier = token.values['identifier']

      if ((not state.InFunction() or state.InConstructor()) and
          not state.InParentheses() and not state.InObjectLiteralDescendant()):
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
              self._HandleError(errors.MISSING_MEMBER_DOCUMENTATION,
                  "No docs found for member '%s'" % identifier,
                  token);
        elif jsdoc and (not state.InConstructor() or
                        identifier.startswith('this.')):
          # We are at the top level and the function/member is documented.
          if identifier.endswith('_') and not identifier.endswith('__'):
            if jsdoc.HasFlag('override'):
              self._HandleError(errors.INVALID_OVERRIDE_PRIVATE,
                  '%s should not override a private member.' % identifier,
                  jsdoc.GetFlag('override').flag_token)
            # Can have a private class which inherits documentation from a
            # public superclass.
            if jsdoc.HasFlag('inheritDoc') and not jsdoc.HasFlag('constructor'):
              self._HandleError(errors.INVALID_INHERIT_DOC_PRIVATE,
                  '%s should not inherit from a private member.' % identifier,
                  jsdoc.GetFlag('inheritDoc').flag_token)
            if (not jsdoc.HasFlag('private') and
                not ('underscore' in jsdoc.suppressions)):
              self._HandleError(errors.MISSING_PRIVATE,
                  'Member "%s" must have @private JsDoc.' %
                  identifier, token)
            if jsdoc.HasFlag('private') and 'underscore' in jsdoc.suppressions:
              self._HandleError(errors.UNNECESSARY_SUPPRESS,
                  '@suppress {underscore} is not necessary with @private',
                  jsdoc.suppressions['underscore'])
          elif jsdoc.HasFlag('private'):
            self._HandleError(errors.EXTRA_PRIVATE,
                'Member "%s" must not have @private JsDoc' %
                identifier, token)

          if ((jsdoc.HasFlag('desc') or jsdoc.HasFlag('hidden'))
              and not identifier.startswith('MSG_')
              and identifier.find('.MSG_') == -1):
            # TODO(user): Update error message to show the actual invalid
            # tag, either @desc or @hidden.
            self._HandleError(errors.INVALID_USE_OF_DESC_TAG,
                'Member "%s" should not have @desc JsDoc' % identifier,
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
          self._HandleError(errors.ILLEGAL_PROTOTYPE_MEMBER_VALUE,
              'Member %s cannot have a non-primitive value' % identifier,
              token)

    elif type == Type.END_PARAMETERS:
      # Find extra space at the end of parameter lists.  We check the token
      # prior to the current one when it is a closing paren.
      if (token.previous and token.previous.type == Type.PARAMETERS
          and self.ENDS_WITH_SPACE.search(token.previous.string)):
        self._HandleError(errors.EXTRA_SPACE, 'Extra space before ")"',
            token.previous)

      jsdoc = state.GetDocComment()
      if state.GetFunction().is_interface:
        if token.previous and token.previous.type == Type.PARAMETERS:
          self._HandleError(errors.INTERFACE_CONSTRUCTOR_CANNOT_HAVE_PARAMS,
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
              self.HandleMissingParameterDoc(token, params_iter.next())

            elif op == 'D':
              # Deletion
              self._HandleError(errors.EXTRA_PARAMETER_DOCUMENTATION,
                  'Found docs for non-existing parameter: "%s"' %
                  docs_iter.next(), token)
            elif op == 'S':
              # Substitution
              self._HandleError(errors.WRONG_PARAMETER_DOCUMENTATION,
                  'Parameter mismatch: got "%s", expected "%s"' %
                  (params_iter.next(), docs_iter.next()), token)

            else:
              # Equality - just advance the iterators
              params_iter.next()
              docs_iter.next()

    elif type == Type.STRING_TEXT:
      # If this is the first token after the start of the string, but it's at
      # the end of a line, we know we have a multi-line string.
      if token.previous.type in (Type.SINGLE_QUOTE_STRING_START,
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
        self._HandleError(errors.EXTRA_SPACE, 'Extra space at end of line',
            token, Position(len(trimmed), len(token.string) - len(trimmed)))

    # This check is also orthogonal since it is based on metadata.
    if token.metadata.is_implied_semicolon:
      self._HandleError(errors.MISSING_SEMICOLON,
          'Missing semicolon at end of line', token)

  def Finalize(self, state, tokenizer_mode):
    last_non_space_token = state.GetLastNonSpaceToken()
    # Check last line for ending with newline.
    if state.GetLastLine() and not (state.GetLastLine().isspace() or
        state.GetLastLine().rstrip('\n\r\f') != state.GetLastLine()):
      self._HandleError(
          errors.FILE_MISSING_NEWLINE,
          'File does not end with new line.  (%s)' % state.GetLastLine(),
          last_non_space_token)

    # Check that the mode is not mid comment, argument list, etc.
    if not tokenizer_mode == Modes.TEXT_MODE:
      self._HandleError(
          errors.FILE_IN_BLOCK,
          'File ended in mode "%s".' % tokenizer_mode,
          last_non_space_token)

    try:
      self._indentation.Finalize()
    except Exception, e:
      self._HandleError(
          errors.FILE_DOES_NOT_PARSE,
          str(e),
          last_non_space_token)

  def GetLongLineExceptions(self):
    """Gets a list of regexps for lines which can be longer than the limit."""
    return []
