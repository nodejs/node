#!/usr/bin/env python
#
# Copyright 2007 The Closure Linter Authors. All Rights Reserved.
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

"""Main class responsible for automatically fixing simple style violations."""

__author__ = 'robbyw@google.com (Robert Walker)'

import re

import gflags as flags
from closure_linter import errors
from closure_linter import javascriptstatetracker
from closure_linter import javascripttokens
from closure_linter import requireprovidesorter
from closure_linter import tokenutil
from closure_linter.common import errorhandler

# Shorthand
Token = javascripttokens.JavaScriptToken
Type = javascripttokens.JavaScriptTokenType

END_OF_FLAG_TYPE = re.compile(r'(}?\s*)$')

# Regex to represent common mistake inverting author name and email as
# @author User Name (user@company)
INVERTED_AUTHOR_SPEC = re.compile(r'(?P<leading_whitespace>\s*)'
                                  '(?P<name>[^(]+)'
                                  '(?P<whitespace_after_name>\s+)'
                                  '\('
                                  '(?P<email>[^\s]+@[^)\s]+)'
                                  '\)'
                                  '(?P<trailing_characters>.*)')

FLAGS = flags.FLAGS
flags.DEFINE_boolean('disable_indentation_fixing', False,
                     'Whether to disable automatic fixing of indentation.')


class ErrorFixer(errorhandler.ErrorHandler):
  """Object that fixes simple style errors."""

  def __init__(self, external_file=None):
    """Initialize the error fixer.

    Args:
      external_file: If included, all output will be directed to this file
          instead of overwriting the files the errors are found in.
    """
    errorhandler.ErrorHandler.__init__(self)

    self._file_name = None
    self._file_token = None
    self._external_file = external_file

  def HandleFile(self, filename, first_token):
    """Notifies this ErrorPrinter that subsequent errors are in filename.

    Args:
      filename: The name of the file about to be checked.
      first_token: The first token in the file.
    """
    self._file_name = filename
    self._file_token = first_token
    self._file_fix_count = 0
    self._file_changed_lines = set()

  def _AddFix(self, tokens):
    """Adds the fix to the internal count.

    Args:
      tokens: The token or sequence of tokens changed to fix an error.
    """
    self._file_fix_count += 1
    if hasattr(tokens, 'line_number'):
      self._file_changed_lines.add(tokens.line_number)
    else:
      for token in tokens:
        self._file_changed_lines.add(token.line_number)

  def HandleError(self, error):
    """Attempts to fix the error.

    Args:
      error: The error object
    """
    code = error.code
    token = error.token

    if code == errors.JSDOC_PREFER_QUESTION_TO_PIPE_NULL:
      iterator = token.attached_object.type_start_token
      if iterator.type == Type.DOC_START_BRACE or iterator.string.isspace():
        iterator = iterator.next

      leading_space = len(iterator.string) - len(iterator.string.lstrip())
      iterator.string = '%s?%s' % (' ' * leading_space,
                                   iterator.string.lstrip())

      # Cover the no outer brace case where the end token is part of the type.
      while iterator and iterator != token.attached_object.type_end_token.next:
        iterator.string = iterator.string.replace(
            'null|', '').replace('|null', '')
        iterator = iterator.next

      # Create a new flag object with updated type info.
      token.attached_object = javascriptstatetracker.JsDocFlag(token)
      self._AddFix(token)

    elif code == errors.JSDOC_MISSING_OPTIONAL_TYPE:
      iterator = token.attached_object.type_end_token
      if iterator.type == Type.DOC_END_BRACE or iterator.string.isspace():
        iterator = iterator.previous

      ending_space = len(iterator.string) - len(iterator.string.rstrip())
      iterator.string = '%s=%s' % (iterator.string.rstrip(),
                                   ' ' * ending_space)

      # Create a new flag object with updated type info.
      token.attached_object = javascriptstatetracker.JsDocFlag(token)
      self._AddFix(token)

    elif code in (errors.MISSING_SEMICOLON_AFTER_FUNCTION,
                  errors.MISSING_SEMICOLON):
      semicolon_token = Token(';', Type.SEMICOLON, token.line,
                              token.line_number)
      tokenutil.InsertTokenAfter(semicolon_token, token)
      token.metadata.is_implied_semicolon = False
      semicolon_token.metadata.is_implied_semicolon = False
      self._AddFix(token)

    elif code in (errors.ILLEGAL_SEMICOLON_AFTER_FUNCTION,
                  errors.REDUNDANT_SEMICOLON,
                  errors.COMMA_AT_END_OF_LITERAL):
      tokenutil.DeleteToken(token)
      self._AddFix(token)

    elif code == errors.INVALID_JSDOC_TAG:
      if token.string == '@returns':
        token.string = '@return'
        self._AddFix(token)

    elif code == errors.FILE_MISSING_NEWLINE:
      # This error is fixed implicitly by the way we restore the file
      self._AddFix(token)

    elif code == errors.MISSING_SPACE:
      if error.position:
        if error.position.IsAtBeginning():
          tokenutil.InsertSpaceTokenAfter(token.previous)
        elif error.position.IsAtEnd(token.string):
          tokenutil.InsertSpaceTokenAfter(token)
        else:
          token.string = error.position.Set(token.string, ' ')
        self._AddFix(token)

    elif code == errors.EXTRA_SPACE:
      if error.position:
        token.string = error.position.Set(token.string, '')
        self._AddFix(token)

    elif code == errors.JSDOC_TAG_DESCRIPTION_ENDS_WITH_INVALID_CHARACTER:
      token.string = error.position.Set(token.string, '.')
      self._AddFix(token)

    elif code == errors.MISSING_LINE:
      if error.position.IsAtBeginning():
        tokenutil.InsertBlankLineAfter(token.previous)
      else:
        tokenutil.InsertBlankLineAfter(token)
      self._AddFix(token)

    elif code == errors.EXTRA_LINE:
      tokenutil.DeleteToken(token)
      self._AddFix(token)

    elif code == errors.WRONG_BLANK_LINE_COUNT:
      if not token.previous:
        # TODO(user): Add an insertBefore method to tokenutil.
        return

      num_lines = error.fix_data
      should_delete = False

      if num_lines < 0:
        num_lines *= -1
        should_delete = True

      for i in xrange(1, num_lines + 1):
        if should_delete:
          # TODO(user): DeleteToken should update line numbers.
          tokenutil.DeleteToken(token.previous)
        else:
          tokenutil.InsertBlankLineAfter(token.previous)
        self._AddFix(token)

    elif code == errors.UNNECESSARY_DOUBLE_QUOTED_STRING:
      end_quote = tokenutil.Search(token, Type.DOUBLE_QUOTE_STRING_END)
      if end_quote:
        single_quote_start = Token(
            "'", Type.SINGLE_QUOTE_STRING_START, token.line, token.line_number)
        single_quote_end = Token(
            "'", Type.SINGLE_QUOTE_STRING_START, end_quote.line,
            token.line_number)

        tokenutil.InsertTokenAfter(single_quote_start, token)
        tokenutil.InsertTokenAfter(single_quote_end, end_quote)
        tokenutil.DeleteToken(token)
        tokenutil.DeleteToken(end_quote)
        self._AddFix([token, end_quote])

    elif code == errors.MISSING_BRACES_AROUND_TYPE:
      fixed_tokens = []
      start_token = token.attached_object.type_start_token

      if start_token.type != Type.DOC_START_BRACE:
        leading_space = (
            len(start_token.string) - len(start_token.string.lstrip()))
        if leading_space:
          start_token = tokenutil.SplitToken(start_token, leading_space)
          # Fix case where start and end token were the same.
          if token.attached_object.type_end_token == start_token.previous:
            token.attached_object.type_end_token = start_token

        new_token = Token('{', Type.DOC_START_BRACE, start_token.line,
                          start_token.line_number)
        tokenutil.InsertTokenAfter(new_token, start_token.previous)
        token.attached_object.type_start_token = new_token
        fixed_tokens.append(new_token)

      end_token = token.attached_object.type_end_token
      if end_token.type != Type.DOC_END_BRACE:
        # If the start token was a brace, the end token will be a
        # FLAG_ENDING_TYPE token, if there wasn't a starting brace then
        # the end token is the last token of the actual type.
        last_type = end_token
        if not fixed_tokens:
          last_type = end_token.previous

        while last_type.string.isspace():
          last_type = last_type.previous

        # If there was no starting brace then a lone end brace wouldn't have
        # been type end token. Now that we've added any missing start brace,
        # see if the last effective type token was an end brace.
        if last_type.type != Type.DOC_END_BRACE:
          trailing_space = (len(last_type.string) -
                            len(last_type.string.rstrip()))
          if trailing_space:
            tokenutil.SplitToken(last_type,
                                 len(last_type.string) - trailing_space)

          new_token = Token('}', Type.DOC_END_BRACE, last_type.line,
                            last_type.line_number)
          tokenutil.InsertTokenAfter(new_token, last_type)
          token.attached_object.type_end_token = new_token
          fixed_tokens.append(new_token)

      self._AddFix(fixed_tokens)

    elif code == errors.GOOG_REQUIRES_NOT_ALPHABETIZED:
      require_start_token = error.fix_data
      sorter = requireprovidesorter.RequireProvideSorter()
      sorter.FixRequires(require_start_token)

      self._AddFix(require_start_token)

    elif code == errors.GOOG_PROVIDES_NOT_ALPHABETIZED:
      provide_start_token = error.fix_data
      sorter = requireprovidesorter.RequireProvideSorter()
      sorter.FixProvides(provide_start_token)

      self._AddFix(provide_start_token)

    elif code == errors.UNNECESSARY_BRACES_AROUND_INHERIT_DOC:
      if token.previous.string == '{' and token.next.string == '}':
        tokenutil.DeleteToken(token.previous)
        tokenutil.DeleteToken(token.next)
        self._AddFix([token])

    elif code == errors.INVALID_AUTHOR_TAG_DESCRIPTION:
      match = INVERTED_AUTHOR_SPEC.match(token.string)
      if match:
        token.string = '%s%s%s(%s)%s' % (match.group('leading_whitespace'),
                                         match.group('email'),
                                         match.group('whitespace_after_name'),
                                         match.group('name'),
                                         match.group('trailing_characters'))
        self._AddFix(token)

    elif (code == errors.WRONG_INDENTATION and
          not FLAGS.disable_indentation_fixing):
      token = tokenutil.GetFirstTokenInSameLine(token)
      actual = error.position.start
      expected = error.position.length

      if token.type in (Type.WHITESPACE, Type.PARAMETERS) and actual != 0:
        token.string = token.string.lstrip() + (' ' * expected)
        self._AddFix([token])
      else:
        # We need to add indentation.
        new_token = Token(' ' * expected, Type.WHITESPACE,
                          token.line, token.line_number)
        # Note that we'll never need to add indentation at the first line,
        # since it will always not be indented.  Therefore it's safe to assume
        # token.previous exists.
        tokenutil.InsertTokenAfter(new_token, token.previous)
        self._AddFix([token])

    elif code in [errors.MALFORMED_END_OF_SCOPE_COMMENT,
                  errors.MISSING_END_OF_SCOPE_COMMENT]:
      # Only fix cases where }); is found with no trailing content on the line
      # other than a comment. Value of 'token' is set to } for this error.
      if (token.type == Type.END_BLOCK and
          token.next.type == Type.END_PAREN and
          token.next.next.type == Type.SEMICOLON):
        current_token = token.next.next.next
        removed_tokens = []
        while current_token and current_token.line_number == token.line_number:
          if current_token.IsAnyType(Type.WHITESPACE,
                                     Type.START_SINGLE_LINE_COMMENT,
                                     Type.COMMENT):
            removed_tokens.append(current_token)
            current_token = current_token.next
          else:
            return

        if removed_tokens:
          tokenutil.DeleteTokens(removed_tokens[0], len(removed_tokens))

        whitespace_token = Token('  ', Type.WHITESPACE, token.line,
                                 token.line_number)
        start_comment_token = Token('//', Type.START_SINGLE_LINE_COMMENT,
                                    token.line, token.line_number)
        comment_token = Token(' goog.scope', Type.COMMENT, token.line,
                              token.line_number)
        insertion_tokens = [whitespace_token, start_comment_token,
                            comment_token]

        tokenutil.InsertTokensAfter(insertion_tokens, token.next.next)
        self._AddFix(removed_tokens + insertion_tokens)

    elif code in [errors.EXTRA_GOOG_PROVIDE, errors.EXTRA_GOOG_REQUIRE]:
      tokens_in_line = tokenutil.GetAllTokensInSameLine(token)
      tokenutil.DeleteTokens(tokens_in_line[0], len(tokens_in_line))
      self._AddFix(tokens_in_line)

    elif code in [errors.MISSING_GOOG_PROVIDE, errors.MISSING_GOOG_REQUIRE]:
      is_provide = code == errors.MISSING_GOOG_PROVIDE
      is_require = code == errors.MISSING_GOOG_REQUIRE

      missing_namespaces = error.fix_data[0]
      need_blank_line = error.fix_data[1]

      if need_blank_line is None:
        # TODO(user): This happens when there are no existing
        # goog.provide or goog.require statements to position new statements
        # relative to. Consider handling this case with a heuristic.
        return

      insert_location = token.previous

      # If inserting a missing require with no existing requires, insert a
      # blank line first.
      if need_blank_line and is_require:
        tokenutil.InsertBlankLineAfter(insert_location)
        insert_location = insert_location.next

      for missing_namespace in missing_namespaces:
        new_tokens = self._GetNewRequireOrProvideTokens(
            is_provide, missing_namespace, insert_location.line_number + 1)
        tokenutil.InsertLineAfter(insert_location, new_tokens)
        insert_location = new_tokens[-1]
        self._AddFix(new_tokens)

      # If inserting a missing provide with no existing provides, insert a
      # blank line after.
      if need_blank_line and is_provide:
        tokenutil.InsertBlankLineAfter(insert_location)

  def _GetNewRequireOrProvideTokens(self, is_provide, namespace, line_number):
    """Returns a list of tokens to create a goog.require/provide statement.

    Args:
      is_provide: True if getting tokens for a provide, False for require.
      namespace: The required or provided namespaces to get tokens for.
      line_number: The line number the new require or provide statement will be
          on.

    Returns:
      Tokens to create a new goog.require or goog.provide statement.
    """
    string = 'goog.require'
    if is_provide:
      string = 'goog.provide'
    line_text = string + '(\'' + namespace + '\');\n'
    return [
        Token(string, Type.IDENTIFIER, line_text, line_number),
        Token('(', Type.START_PAREN, line_text, line_number),
        Token('\'', Type.SINGLE_QUOTE_STRING_START, line_text, line_number),
        Token(namespace, Type.STRING_TEXT, line_text, line_number),
        Token('\'', Type.SINGLE_QUOTE_STRING_END, line_text, line_number),
        Token(')', Type.END_PAREN, line_text, line_number),
        Token(';', Type.SEMICOLON, line_text, line_number)
        ]

  def FinishFile(self):
    """Called when the current file has finished style checking.

    Used to go back and fix any errors in the file.
    """
    if self._file_fix_count:
      f = self._external_file
      if not f:
        print 'Fixed %d errors in %s' % (self._file_fix_count, self._file_name)
        f = open(self._file_name, 'w')

      token = self._file_token
      char_count = 0
      while token:
        f.write(token.string)
        char_count += len(token.string)

        if token.IsLastInLine():
          f.write('\n')
          if char_count > 80 and token.line_number in self._file_changed_lines:
            print 'WARNING: Line %d of %s is now longer than 80 characters.' % (
                token.line_number, self._file_name)

          char_count = 0

        token = token.next

      if not self._external_file:
        # Close the file if we created it
        f.close()
