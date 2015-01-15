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

# Allow non-Google copyright
# pylint: disable=g-bad-file-header

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
                                  r'(?P<name>[^(]+)'
                                  r'(?P<whitespace_after_name>\s+)'
                                  r'\('
                                  r'(?P<email>[^\s]+@[^)\s]+)'
                                  r'\)'
                                  r'(?P<trailing_characters>.*)')

FLAGS = flags.FLAGS
flags.DEFINE_boolean('disable_indentation_fixing', False,
                     'Whether to disable automatic fixing of indentation.')
flags.DEFINE_list('fix_error_codes', [], 'A list of specific error codes to '
                  'fix. Defaults to all supported error codes when empty. '
                  'See errors.py for a list of error codes.')


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

    try:
      self._fix_error_codes = set([errors.ByName(error.upper()) for error in
                                   FLAGS.fix_error_codes])
    except KeyError as ke:
      raise ValueError('Unknown error code ' + ke.args[0])

  def HandleFile(self, filename, first_token):
    """Notifies this ErrorPrinter that subsequent errors are in filename.

    Args:
      filename: The name of the file about to be checked.
      first_token: The first token in the file.
    """
    self._file_name = filename
    self._file_is_html = filename.endswith('.html') or filename.endswith('.htm')
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

  def _FixJsDocPipeNull(self, js_type):
    """Change number|null or null|number to ?number.

    Args:
      js_type: The typeannotation.TypeAnnotation instance to fix.
    """

    # Recurse into all sub_types if the error was at a deeper level.
    map(self._FixJsDocPipeNull, js_type.IterTypes())

    if js_type.type_group and len(js_type.sub_types) == 2:
      # Find and remove the null sub_type:
      sub_type = None
      for sub_type in js_type.sub_types:
        if sub_type.identifier == 'null':
          map(tokenutil.DeleteToken, sub_type.tokens)
          self._AddFix(sub_type.tokens)
          break
      else:
        return

      first_token = js_type.FirstToken()
      question_mark = Token('?', Type.DOC_TYPE_MODIFIER, first_token.line,
                            first_token.line_number)
      tokenutil.InsertTokenBefore(question_mark, first_token)
      js_type.tokens.insert(0, question_mark)
      js_type.tokens.remove(sub_type)
      js_type.or_null = True

      # Now also remove the separator, which is in the parent's token list,
      # either before or after the sub_type, there is exactly one. Scan for it.
      for token in js_type.tokens:
        if (token and isinstance(token, Token) and
            token.type == Type.DOC_TYPE_MODIFIER and token.string == '|'):
          tokenutil.DeleteToken(token)
          self._AddFix(token)
          break

  def HandleError(self, error):
    """Attempts to fix the error.

    Args:
      error: The error object
    """
    code = error.code
    token = error.token

    if self._fix_error_codes and code not in self._fix_error_codes:
      return

    if code == errors.JSDOC_PREFER_QUESTION_TO_PIPE_NULL:
      self._FixJsDocPipeNull(token.attached_object.jstype)

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

    elif code == errors.JSDOC_MISSING_VAR_ARGS_TYPE:
      iterator = token.attached_object.type_start_token
      if iterator.type == Type.DOC_START_BRACE or iterator.string.isspace():
        iterator = iterator.next

      starting_space = len(iterator.string) - len(iterator.string.lstrip())
      iterator.string = '%s...%s' % (' ' * starting_space,
                                     iterator.string.lstrip())

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
      self._DeleteToken(token)
      self._AddFix(token)

    elif code == errors.INVALID_JSDOC_TAG:
      if token.string == '@returns':
        token.string = '@return'
        self._AddFix(token)

    elif code == errors.FILE_MISSING_NEWLINE:
      # This error is fixed implicitly by the way we restore the file
      self._AddFix(token)

    elif code == errors.MISSING_SPACE:
      if error.fix_data:
        token.string = error.fix_data
        self._AddFix(token)
      elif error.position:
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

    elif code == errors.MISSING_LINE:
      if error.position.IsAtBeginning():
        tokenutil.InsertBlankLineAfter(token.previous)
      else:
        tokenutil.InsertBlankLineAfter(token)
      self._AddFix(token)

    elif code == errors.EXTRA_LINE:
      self._DeleteToken(token)
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

      for unused_i in xrange(1, num_lines + 1):
        if should_delete:
          # TODO(user): DeleteToken should update line numbers.
          self._DeleteToken(token.previous)
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
        self._DeleteToken(token)
        self._DeleteToken(end_quote)
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

    elif code == errors.LINE_STARTS_WITH_OPERATOR:
      # Remove whitespace following the operator so the line starts clean.
      self._StripSpace(token, before=False)

      # Remove the operator.
      tokenutil.DeleteToken(token)
      self._AddFix(token)

      insertion_point = tokenutil.GetPreviousCodeToken(token)

      # Insert a space between the previous token and the new operator.
      space = Token(' ', Type.WHITESPACE, insertion_point.line,
                    insertion_point.line_number)
      tokenutil.InsertTokenAfter(space, insertion_point)

      # Insert the operator on the end of the previous line.
      new_token = Token(token.string, token.type, insertion_point.line,
                        insertion_point.line_number)
      tokenutil.InsertTokenAfter(new_token, space)
      self._AddFix(new_token)

    elif code == errors.LINE_ENDS_WITH_DOT:
      # Remove whitespace preceding the operator to remove trailing whitespace.
      self._StripSpace(token, before=True)

      # Remove the dot.
      tokenutil.DeleteToken(token)
      self._AddFix(token)

      insertion_point = tokenutil.GetNextCodeToken(token)

      # Insert the dot at the beginning of the next line of code.
      new_token = Token(token.string, token.type, insertion_point.line,
                        insertion_point.line_number)
      tokenutil.InsertTokenBefore(new_token, insertion_point)
      self._AddFix(new_token)

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
        self._DeleteToken(token.previous)
        self._DeleteToken(token.next)
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

      # Cases where first token is param but with leading spaces.
      if (len(token.string.lstrip()) == len(token.string) - actual and
          token.string.lstrip()):
        token.string = token.string.lstrip()
        actual = 0

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
          self._DeleteTokens(removed_tokens[0], len(removed_tokens))

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
      num_delete_tokens = len(tokens_in_line)
      # If line being deleted is preceded and succeed with blank lines then
      # delete one blank line also.
      if (tokens_in_line[0].previous and tokens_in_line[-1].next
          and tokens_in_line[0].previous.type == Type.BLANK_LINE
          and tokens_in_line[-1].next.type == Type.BLANK_LINE):
        num_delete_tokens += 1
      self._DeleteTokens(tokens_in_line[0], num_delete_tokens)
      self._AddFix(tokens_in_line)

    elif code in [errors.MISSING_GOOG_PROVIDE, errors.MISSING_GOOG_REQUIRE]:
      missing_namespaces = error.fix_data[0]
      need_blank_line = error.fix_data[1] or (not token.previous)

      insert_location = Token('', Type.NORMAL, '', token.line_number - 1)
      dummy_first_token = insert_location
      tokenutil.InsertTokenBefore(insert_location, token)

      # If inserting a blank line check blank line does not exist before
      # token to avoid extra blank lines.
      if (need_blank_line and insert_location.previous
          and insert_location.previous.type != Type.BLANK_LINE):
        tokenutil.InsertBlankLineAfter(insert_location)
        insert_location = insert_location.next

      for missing_namespace in missing_namespaces:
        new_tokens = self._GetNewRequireOrProvideTokens(
            code == errors.MISSING_GOOG_PROVIDE,
            missing_namespace, insert_location.line_number + 1)
        tokenutil.InsertLineAfter(insert_location, new_tokens)
        insert_location = new_tokens[-1]
        self._AddFix(new_tokens)

      # If inserting a blank line check blank line does not exist after
      # token to avoid extra blank lines.
      if (need_blank_line and insert_location.next
          and insert_location.next.type != Type.BLANK_LINE):
        tokenutil.InsertBlankLineAfter(insert_location)

      tokenutil.DeleteToken(dummy_first_token)

  def _StripSpace(self, token, before):
    """Strip whitespace tokens either preceding or following the given token.

    Args:
      token: The token.
      before: If true, strip space before the token, if false, after it.
    """
    token = token.previous if before else token.next
    while token and token.type == Type.WHITESPACE:
      tokenutil.DeleteToken(token)
      token = token.previous if before else token.next

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

  def _DeleteToken(self, token):
    """Deletes the specified token from the linked list of tokens.

    Updates instance variables pointing to tokens such as _file_token if
    they reference the deleted token.

    Args:
      token: The token to delete.
    """
    if token == self._file_token:
      self._file_token = token.next

    tokenutil.DeleteToken(token)

  def _DeleteTokens(self, token, token_count):
    """Deletes the given number of tokens starting with the given token.

    Updates instance variables pointing to tokens such as _file_token if
    they reference the deleted token.

    Args:
      token: The first token to delete.
      token_count: The total number of tokens to delete.
    """
    if token == self._file_token:
      for unused_i in xrange(token_count):
        self._file_token = self._file_token.next

    tokenutil.DeleteTokens(token, token_count)

  def FinishFile(self):
    """Called when the current file has finished style checking.

    Used to go back and fix any errors in the file. It currently supports both
    js and html files. For js files it does a simple dump of all tokens, but in
    order to support html file, we need to merge the original file with the new
    token set back together. This works because the tokenized html file is the
    original html file with all non js lines kept but blanked out with one blank
    line token per line of html.
    """
    if self._file_fix_count:
      # Get the original file content for html.
      if self._file_is_html:
        f = open(self._file_name, 'r')
        original_lines = f.readlines()
        f.close()

      f = self._external_file
      if not f:
        error_noun = 'error' if self._file_fix_count == 1 else 'errors'
        print 'Fixed %d %s in %s' % (
            self._file_fix_count, error_noun, self._file_name)
        f = open(self._file_name, 'w')

      token = self._file_token
      # Finding the first not deleted token.
      while token.is_deleted:
        token = token.next
      # If something got inserted before first token (e.g. due to sorting)
      # then move to start. Bug 8398202.
      while token.previous:
        token = token.previous
      char_count = 0
      line = ''
      while token:
        line += token.string
        char_count += len(token.string)

        if token.IsLastInLine():
          # We distinguish if a blank line in html was from stripped original
          # file or newly added error fix by looking at the "org_line_number"
          # field on the token. It is only set in the tokenizer, so for all
          # error fixes, the value should be None.
          if (line or not self._file_is_html or
              token.orig_line_number is None):
            f.write(line)
            f.write('\n')
          else:
            f.write(original_lines[token.orig_line_number - 1])
          line = ''
          if char_count > 80 and token.line_number in self._file_changed_lines:
            print 'WARNING: Line %d of %s is now longer than 80 characters.' % (
                token.line_number, self._file_name)

          char_count = 0

        token = token.next

      if not self._external_file:
        # Close the file if we created it
        f.close()
