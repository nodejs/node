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

"""Light weight EcmaScript state tracker that reads tokens and tracks state."""

__author__ = ('robbyw@google.com (Robert Walker)',
              'ajp@google.com (Andy Perelson)')

import re

from closure_linter import javascripttokenizer
from closure_linter import javascripttokens
from closure_linter import tokenutil

# Shorthand
Type = javascripttokens.JavaScriptTokenType


class DocFlag(object):
  """Generic doc flag object.

  Attribute:
    flag_type: param, return, define, type, etc.
    flag_token: The flag token.
    type_start_token: The first token specifying the flag type,
      including braces.
    type_end_token: The last token specifying the flag type,
      including braces.
    type: The type spec.
    name_token: The token specifying the flag name.
    name: The flag name
    description_start_token: The first token in the description.
    description_end_token: The end token in the description.
    description: The description.
  """

  # Please keep these lists alphabetized.

  # The list of standard jsdoc tags is from
  STANDARD_DOC = frozenset([
      'author',
      'bug',
      'const',
      'constructor',
      'define',
      'deprecated',
      'enum',
      'export',
      'extends',
      'externs',
      'fileoverview',
      'implements',
      'implicitCast',
      'interface',
      'lends',
      'license',
      'noalias',
      'nocompile',
      'nosideeffects',
      'override',
      'owner',
      'param',
      'preserve',
      'private',
      'return',
      'see',
      'supported',
      'template',
      'this',
      'type',
      'typedef',
      ])

  ANNOTATION = frozenset(['preserveTry', 'suppress'])

  LEGAL_DOC = STANDARD_DOC | ANNOTATION

  # Includes all Closure Compiler @suppress types.
  # Not all of these annotations are interpreted by Closure Linter.
  #
  # Specific cases:
  # - accessControls is supported by the compiler at the expression
  #   and method level to suppress warnings about private/protected
  #   access (method level applies to all references in the method).
  #   The linter mimics the compiler behavior.
  SUPPRESS_TYPES = frozenset([
      'accessControls',
      'ambiguousFunctionDecl',
      'checkRegExp',
      'checkTypes',
      'checkVars',
      'const',
      'constantProperty',
      'deprecated',
      'duplicate',
      'es5Strict',
      'externsValidation',
      'extraProvide',
      'extraRequire',
      'fileoverviewTags',
      'globalThis',
      'internetExplorerChecks',
      'invalidCasts',
      'missingProperties',
      'missingProvide',
      'missingRequire',
      'nonStandardJsDocs',
      'strictModuleDepCheck',
      'tweakValidation',
      'typeInvalidation',
      'undefinedNames',
      'undefinedVars',
      'underscore',
      'unknownDefines',
      'uselessCode',
      'visibility',
      'with'])

  HAS_DESCRIPTION = frozenset([
    'define', 'deprecated', 'desc', 'fileoverview', 'license', 'param',
    'preserve', 'return', 'supported'])

  HAS_TYPE = frozenset([
      'define', 'enum', 'extends', 'implements', 'param', 'return', 'type',
      'suppress'])

  TYPE_ONLY = frozenset(['enum', 'extends', 'implements',  'suppress', 'type'])

  HAS_NAME = frozenset(['param'])

  EMPTY_COMMENT_LINE = re.compile(r'^\s*\*?\s*$')
  EMPTY_STRING = re.compile(r'^\s*$')

  def __init__(self, flag_token):
    """Creates the DocFlag object and attaches it to the given start token.

    Args:
      flag_token: The starting token of the flag.
    """
    self.flag_token = flag_token
    self.flag_type = flag_token.string.strip().lstrip('@')

    # Extract type, if applicable.
    self.type = None
    self.type_start_token = None
    self.type_end_token = None
    if self.flag_type in self.HAS_TYPE:
      brace = tokenutil.SearchUntil(flag_token, [Type.DOC_START_BRACE],
                                    Type.FLAG_ENDING_TYPES)
      if brace:
        end_token, contents = _GetMatchingEndBraceAndContents(brace)
        self.type = contents
        self.type_start_token = brace
        self.type_end_token = end_token
      elif (self.flag_type in self.TYPE_ONLY and
          flag_token.next.type not in Type.FLAG_ENDING_TYPES):
        self.type_start_token = flag_token.next
        self.type_end_token, self.type = _GetEndTokenAndContents(
            self.type_start_token)
        if self.type is not None:
          self.type = self.type.strip()

    # Extract name, if applicable.
    self.name_token = None
    self.name = None
    if self.flag_type in self.HAS_NAME:
      # Handle bad case, name could be immediately after flag token.
      self.name_token = _GetNextIdentifierToken(flag_token)

      # Handle good case, if found token is after type start, look for
      # identifier after type end, since types contain identifiers.
      if (self.type and self.name_token and
          tokenutil.Compare(self.name_token, self.type_start_token) > 0):
        self.name_token = _GetNextIdentifierToken(self.type_end_token)

      if self.name_token:
        self.name = self.name_token.string

    # Extract description, if applicable.
    self.description_start_token = None
    self.description_end_token = None
    self.description = None
    if self.flag_type in self.HAS_DESCRIPTION:
      search_start_token = flag_token
      if self.name_token and self.type_end_token:
        if tokenutil.Compare(self.type_end_token, self.name_token) > 0:
          search_start_token = self.type_end_token
        else:
          search_start_token = self.name_token
      elif self.name_token:
        search_start_token = self.name_token
      elif self.type:
        search_start_token = self.type_end_token

      interesting_token = tokenutil.Search(search_start_token,
          Type.FLAG_DESCRIPTION_TYPES | Type.FLAG_ENDING_TYPES)
      if interesting_token.type in Type.FLAG_DESCRIPTION_TYPES:
        self.description_start_token = interesting_token
        self.description_end_token, self.description = (
            _GetEndTokenAndContents(interesting_token))


class DocComment(object):
  """JavaScript doc comment object.

  Attributes:
    ordered_params: Ordered list of parameters documented.
    start_token: The token that starts the doc comment.
    end_token: The token that ends the doc comment.
    suppressions: Map of suppression type to the token that added it.
  """
  def __init__(self, start_token):
    """Create the doc comment object.

    Args:
      start_token: The first token in the doc comment.
    """
    self.__params = {}
    self.ordered_params = []
    self.__flags = {}
    self.start_token = start_token
    self.end_token = None
    self.suppressions = {}
    self.invalidated = False

  def Invalidate(self):
    """Indicate that the JSDoc is well-formed but we had problems parsing it.

    This is a short-circuiting mechanism so that we don't emit false
    positives about well-formed doc comments just because we don't support
    hot new syntaxes.
    """
    self.invalidated = True

  def IsInvalidated(self):
    """Test whether Invalidate() has been called."""
    return self.invalidated

  def AddParam(self, name, param_type):
    """Add a new documented parameter.

    Args:
      name: The name of the parameter to document.
      param_type: The parameter's declared JavaScript type.
    """
    self.ordered_params.append(name)
    self.__params[name] = param_type

  def AddSuppression(self, token):
    """Add a new error suppression flag.

    Args:
      token: The suppression flag token.
    """
    #TODO(user): Error if no braces
    brace = tokenutil.SearchUntil(token, [Type.DOC_START_BRACE],
                                  [Type.DOC_FLAG])
    if brace:
      end_token, contents = _GetMatchingEndBraceAndContents(brace)
      for suppression in contents.split('|'):
        self.suppressions[suppression] = token

  def SuppressionOnly(self):
    """Returns whether this comment contains only suppression flags."""
    for flag_type in self.__flags.keys():
      if flag_type != 'suppress':
        return False
    return True

  def AddFlag(self, flag):
    """Add a new document flag.

    Args:
      flag: DocFlag object.
    """
    self.__flags[flag.flag_type] = flag

  def InheritsDocumentation(self):
    """Test if the jsdoc implies documentation inheritance.

    Returns:
        True if documentation may be pulled off the superclass.
    """
    return self.HasFlag('inheritDoc') or self.HasFlag('override')

  def HasFlag(self, flag_type):
    """Test if the given flag has been set.

    Args:
      flag_type: The type of the flag to check.

    Returns:
      True if the flag is set.
    """
    return flag_type in self.__flags

  def GetFlag(self, flag_type):
    """Gets the last flag of the given type.

    Args:
      flag_type: The type of the flag to get.

    Returns:
      The last instance of the given flag type in this doc comment.
    """
    return self.__flags[flag_type]

  def CompareParameters(self, params):
    """Computes the edit distance and list from the function params to the docs.

    Uses the Levenshtein edit distance algorithm, with code modified from
    http://en.wikibooks.org/wiki/Algorithm_implementation/Strings/Levenshtein_distance#Python

    Args:
      params: The parameter list for the function declaration.

    Returns:
      The edit distance, the edit list.
    """
    source_len, target_len = len(self.ordered_params), len(params)
    edit_lists = [[]]
    distance = [[]]
    for i in range(target_len+1):
      edit_lists[0].append(['I'] * i)
      distance[0].append(i)

    for j in range(1, source_len+1):
      edit_lists.append([['D'] * j])
      distance.append([j])

    for i in range(source_len):
      for j in range(target_len):
        cost = 1
        if self.ordered_params[i] == params[j]:
          cost = 0

        deletion = distance[i][j+1] + 1
        insertion = distance[i+1][j] + 1
        substitution = distance[i][j] + cost

        edit_list = None
        best = None
        if deletion <= insertion and deletion <= substitution:
          # Deletion is best.
          best = deletion
          edit_list = list(edit_lists[i][j+1])
          edit_list.append('D')

        elif insertion <= substitution:
          # Insertion is best.
          best = insertion
          edit_list = list(edit_lists[i+1][j])
          edit_list.append('I')
          edit_lists[i+1].append(edit_list)

        else:
          # Substitution is best.
          best = substitution
          edit_list = list(edit_lists[i][j])
          if cost:
            edit_list.append('S')
          else:
            edit_list.append('=')

        edit_lists[i+1].append(edit_list)
        distance[i+1].append(best)

    return distance[source_len][target_len], edit_lists[source_len][target_len]

  def __repr__(self):
    """Returns a string representation of this object.

    Returns:
      A string representation of this object.
    """
    return '<DocComment: %s, %s>' % (str(self.__params), str(self.__flags))


#
# Helper methods used by DocFlag and DocComment to parse out flag information.
#


def _GetMatchingEndBraceAndContents(start_brace):
  """Returns the matching end brace and contents between the two braces.

  If any FLAG_ENDING_TYPE token is encountered before a matching end brace, then
  that token is used as the matching ending token. Contents will have all
  comment prefixes stripped out of them, and all comment prefixes in between the
  start and end tokens will be split out into separate DOC_PREFIX tokens.

  Args:
    start_brace: The DOC_START_BRACE token immediately before desired contents.

  Returns:
    The matching ending token (DOC_END_BRACE or FLAG_ENDING_TYPE) and a string
    of the contents between the matching tokens, minus any comment prefixes.
  """
  open_count = 1
  close_count = 0
  contents = []

  # We don't consider the start brace part of the type string.
  token = start_brace.next
  while open_count != close_count:
    if token.type == Type.DOC_START_BRACE:
      open_count += 1
    elif token.type == Type.DOC_END_BRACE:
      close_count += 1

    if token.type != Type.DOC_PREFIX:
      contents.append(token.string)

    if token.type in Type.FLAG_ENDING_TYPES:
      break
    token = token.next

  #Don't include the end token (end brace, end doc comment, etc.) in type.
  token = token.previous
  contents = contents[:-1]

  return token, ''.join(contents)


def _GetNextIdentifierToken(start_token):
  """Searches for and returns the first identifier at the beginning of a token.

  Searches each token after the start to see if it starts with an identifier.
  If found, will split the token into at most 3 piecies: leading whitespace,
  identifier, rest of token, returning the identifier token. If no identifier is
  found returns None and changes no tokens. Search is abandoned when a
  FLAG_ENDING_TYPE token is found.

  Args:
    start_token: The token to start searching after.

  Returns:
    The identifier token is found, None otherwise.
  """
  token = start_token.next

  while token and not token.type in Type.FLAG_ENDING_TYPES:
    match = javascripttokenizer.JavaScriptTokenizer.IDENTIFIER.match(
        token.string)
    if (match is not None and token.type == Type.COMMENT and
        len(token.string) == len(match.group(0))):
      return token

    token = token.next

  return None


def _GetEndTokenAndContents(start_token):
  """Returns last content token and all contents before FLAG_ENDING_TYPE token.

  Comment prefixes are split into DOC_PREFIX tokens and stripped from the
  returned contents.

  Args:
    start_token: The token immediately before the first content token.

  Returns:
    The last content token and a string of all contents including start and
    end tokens, with comment prefixes stripped.
  """
  iterator = start_token
  last_line = iterator.line_number
  last_token = None
  contents = ''
  doc_depth = 0
  while not iterator.type in Type.FLAG_ENDING_TYPES or doc_depth > 0:
    if (iterator.IsFirstInLine() and
        DocFlag.EMPTY_COMMENT_LINE.match(iterator.line)):
      # If we have a blank comment line, consider that an implicit
      # ending of the description. This handles a case like:
      #
      # * @return {boolean} True
      # *
      # * Note: This is a sentence.
      #
      # The note is not part of the @return description, but there was
      # no definitive ending token. Rather there was a line containing
      # only a doc comment prefix or whitespace.
      break

    # b/2983692
    # don't prematurely match against a @flag if inside a doc flag
    # need to think about what is the correct behavior for unterminated
    # inline doc flags
    if (iterator.type == Type.DOC_START_BRACE and
        iterator.next.type == Type.DOC_INLINE_FLAG):
      doc_depth += 1
    elif (iterator.type == Type.DOC_END_BRACE and
        doc_depth > 0):
      doc_depth -= 1

    if iterator.type in Type.FLAG_DESCRIPTION_TYPES:
      contents += iterator.string
      last_token = iterator

    iterator = iterator.next
    if iterator.line_number != last_line:
      contents += '\n'
      last_line = iterator.line_number

  end_token = last_token
  if DocFlag.EMPTY_STRING.match(contents):
    contents = None
  else:
    # Strip trailing newline.
    contents = contents[:-1]

  return end_token, contents


class Function(object):
  """Data about a JavaScript function.

  Attributes:
    block_depth: Block depth the function began at.
    doc: The DocComment associated with the function.
    has_return: If the function has a return value.
    has_this: If the function references the 'this' object.
    is_assigned: If the function is part of an assignment.
    is_constructor: If the function is a constructor.
    name: The name of the function, whether given in the function keyword or
        as the lvalue the function is assigned to.
  """

  def __init__(self, block_depth, is_assigned, doc, name):
    self.block_depth = block_depth
    self.is_assigned = is_assigned
    self.is_constructor = doc and doc.HasFlag('constructor')
    self.is_interface = doc and doc.HasFlag('interface')
    self.has_return = False
    self.has_throw = False
    self.has_this = False
    self.name = name
    self.doc = doc


class StateTracker(object):
  """EcmaScript state tracker.

  Tracks block depth, function names, etc. within an EcmaScript token stream.
  """

  OBJECT_LITERAL = 'o'
  CODE = 'c'

  def __init__(self, doc_flag=DocFlag):
    """Initializes a JavaScript token stream state tracker.

    Args:
      doc_flag: An optional custom DocFlag used for validating
          documentation flags.
    """
    self._doc_flag = doc_flag
    self.Reset()

  def Reset(self):
    """Resets the state tracker to prepare for processing a new page."""
    self._block_depth = 0
    self._is_block_close = False
    self._paren_depth = 0
    self._functions = []
    self._functions_by_name = {}
    self._last_comment = None
    self._doc_comment = None
    self._cumulative_params = None
    self._block_types = []
    self._last_non_space_token = None
    self._last_line = None
    self._first_token = None
    self._documented_identifiers = set()

  def InFunction(self):
    """Returns true if the current token is within a function.

    Returns:
      True if the current token is within a function.
    """
    return bool(self._functions)

  def InConstructor(self):
    """Returns true if the current token is within a constructor.

    Returns:
      True if the current token is within a constructor.
    """
    return self.InFunction() and self._functions[-1].is_constructor

  def InInterfaceMethod(self):
    """Returns true if the current token is within an interface method.

    Returns:
      True if the current token is within an interface method.
    """
    if self.InFunction():
      if self._functions[-1].is_interface:
        return True
      else:
        name = self._functions[-1].name
        prototype_index = name.find('.prototype.')
        if prototype_index != -1:
          class_function_name = name[0:prototype_index]
          if (class_function_name in self._functions_by_name and
              self._functions_by_name[class_function_name].is_interface):
            return True

    return False

  def InTopLevelFunction(self):
    """Returns true if the current token is within a top level function.

    Returns:
      True if the current token is within a top level function.
    """
    return len(self._functions) == 1 and self.InTopLevel()

  def InAssignedFunction(self):
    """Returns true if the current token is within a function variable.

    Returns:
      True if if the current token is within a function variable
    """
    return self.InFunction() and self._functions[-1].is_assigned

  def IsFunctionOpen(self):
    """Returns true if the current token is a function block open.

    Returns:
      True if the current token is a function block open.
    """
    return (self._functions and
            self._functions[-1].block_depth == self._block_depth - 1)

  def IsFunctionClose(self):
    """Returns true if the current token is a function block close.

    Returns:
      True if the current token is a function block close.
    """
    return (self._functions and
            self._functions[-1].block_depth == self._block_depth)

  def InBlock(self):
    """Returns true if the current token is within a block.

    Returns:
      True if the current token is within a block.
    """
    return bool(self._block_depth)

  def IsBlockClose(self):
    """Returns true if the current token is a block close.

    Returns:
      True if the current token is a block close.
    """
    return self._is_block_close

  def InObjectLiteral(self):
    """Returns true if the current token is within an object literal.

    Returns:
      True if the current token is within an object literal.
    """
    return self._block_depth and self._block_types[-1] == self.OBJECT_LITERAL

  def InObjectLiteralDescendant(self):
    """Returns true if the current token has an object literal ancestor.

    Returns:
      True if the current token has an object literal ancestor.
    """
    return self.OBJECT_LITERAL in self._block_types

  def InParentheses(self):
    """Returns true if the current token is within parentheses.

    Returns:
      True if the current token is within parentheses.
    """
    return bool(self._paren_depth)

  def InTopLevel(self):
    """Whether we are at the top level in the class.

    This function call is language specific.  In some languages like
    JavaScript, a function is top level if it is not inside any parenthesis.
    In languages such as ActionScript, a function is top level if it is directly
    within a class.
    """
    raise TypeError('Abstract method InTopLevel not implemented')

  def GetBlockType(self, token):
    """Determine the block type given a START_BLOCK token.

    Code blocks come after parameters, keywords  like else, and closing parens.

    Args:
      token: The current token. Can be assumed to be type START_BLOCK.
    Returns:
      Code block type for current token.
    """
    raise TypeError('Abstract method GetBlockType not implemented')

  def GetParams(self):
    """Returns the accumulated input params as an array.

    In some EcmasSript languages, input params are specified like
    (param:Type, param2:Type2, ...)
    in other they are specified just as
    (param, param2)
    We handle both formats for specifying parameters here and leave
    it to the compilers for each language to detect compile errors.
    This allows more code to be reused between lint checkers for various
    EcmaScript languages.

    Returns:
      The accumulated input params as an array.
    """
    params = []
    if self._cumulative_params:
      params = re.compile(r'\s+').sub('', self._cumulative_params).split(',')
      # Strip out the type from parameters of the form name:Type.
      params = map(lambda param: param.split(':')[0], params)

    return params

  def GetLastComment(self):
    """Return the last plain comment that could be used as documentation.

    Returns:
      The last plain comment that could be used as documentation.
    """
    return self._last_comment

  def GetDocComment(self):
    """Return the most recent applicable documentation comment.

    Returns:
      The last applicable documentation comment.
    """
    return self._doc_comment

  def HasDocComment(self, identifier):
    """Returns whether the identifier has been documented yet.

    Args:
      identifier: The identifier.

    Returns:
      Whether the identifier has been documented yet.
    """
    return identifier in self._documented_identifiers

  def InDocComment(self):
    """Returns whether the current token is in a doc comment.

    Returns:
      Whether the current token is in a doc comment.
    """
    return self._doc_comment and self._doc_comment.end_token is None

  def GetDocFlag(self):
    """Returns the current documentation flags.

    Returns:
      The current documentation flags.
    """
    return self._doc_flag

  def IsTypeToken(self, t):
    if self.InDocComment() and t.type not in (Type.START_DOC_COMMENT,
        Type.DOC_FLAG, Type.DOC_INLINE_FLAG, Type.DOC_PREFIX):
      f = tokenutil.SearchUntil(t, [Type.DOC_FLAG], [Type.START_DOC_COMMENT],
                                None, True)
      if f and f.attached_object.type_start_token is not None:
        return (tokenutil.Compare(t, f.attached_object.type_start_token) > 0 and
                tokenutil.Compare(t, f.attached_object.type_end_token) < 0)
    return False

  def GetFunction(self):
    """Return the function the current code block is a part of.

    Returns:
      The current Function object.
    """
    if self._functions:
      return self._functions[-1]

  def GetBlockDepth(self):
    """Return the block depth.

    Returns:
      The current block depth.
    """
    return self._block_depth

  def GetLastNonSpaceToken(self):
    """Return the last non whitespace token."""
    return self._last_non_space_token

  def GetLastLine(self):
    """Return the last line."""
    return self._last_line

  def GetFirstToken(self):
    """Return the very first token in the file."""
    return self._first_token

  def HandleToken(self, token, last_non_space_token):
    """Handles the given token and updates state.

    Args:
      token: The token to handle.
      last_non_space_token:
    """
    self._is_block_close = False

    if not self._first_token:
      self._first_token = token

    # Track block depth.
    type = token.type
    if type == Type.START_BLOCK:
      self._block_depth += 1

      # Subclasses need to handle block start very differently because
      # whether a block is a CODE or OBJECT_LITERAL block varies significantly
      # by language.
      self._block_types.append(self.GetBlockType(token))

    # Track block depth.
    elif type == Type.END_BLOCK:
      self._is_block_close = not self.InObjectLiteral()
      self._block_depth -= 1
      self._block_types.pop()

    # Track parentheses depth.
    elif type == Type.START_PAREN:
      self._paren_depth += 1

    # Track parentheses depth.
    elif type == Type.END_PAREN:
      self._paren_depth -= 1

    elif type == Type.COMMENT:
      self._last_comment = token.string

    elif type == Type.START_DOC_COMMENT:
      self._last_comment = None
      self._doc_comment = DocComment(token)

    elif type == Type.END_DOC_COMMENT:
      self._doc_comment.end_token = token

    elif type in (Type.DOC_FLAG, Type.DOC_INLINE_FLAG):
      flag = self._doc_flag(token)
      token.attached_object = flag
      self._doc_comment.AddFlag(flag)

      if flag.flag_type == 'param' and flag.name:
        self._doc_comment.AddParam(flag.name, flag.type)
      elif flag.flag_type == 'suppress':
        self._doc_comment.AddSuppression(token)

    elif type == Type.FUNCTION_DECLARATION:
      last_code = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES, None,
                                         True)
      doc = None
      # Only functions outside of parens are eligible for documentation.
      if not self._paren_depth:
        doc = self._doc_comment

      name = ''
      is_assigned = last_code and (last_code.IsOperator('=') or
          last_code.IsOperator('||') or last_code.IsOperator('&&') or
          (last_code.IsOperator(':') and not self.InObjectLiteral()))
      if is_assigned:
        # TODO(robbyw): This breaks for x[2] = ...
        # Must use loop to find full function name in the case of line-wrapped
        # declarations (bug 1220601) like:
        # my.function.foo.
        #   bar = function() ...
        identifier = tokenutil.Search(last_code, Type.SIMPLE_LVALUE, None, True)
        while identifier and identifier.type in (
            Type.IDENTIFIER, Type.SIMPLE_LVALUE):
          name = identifier.string + name
          # Traverse behind us, skipping whitespace and comments.
          while True:
            identifier = identifier.previous
            if not identifier or not identifier.type in Type.NON_CODE_TYPES:
              break

      else:
        next_token = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES)
        while next_token and next_token.IsType(Type.FUNCTION_NAME):
          name += next_token.string
          next_token = tokenutil.Search(next_token, Type.FUNCTION_NAME, 2)

      function = Function(self._block_depth, is_assigned, doc, name)
      self._functions.append(function)
      self._functions_by_name[name] = function

    elif type == Type.START_PARAMETERS:
      self._cumulative_params = ''

    elif type == Type.PARAMETERS:
      self._cumulative_params += token.string

    elif type == Type.KEYWORD and token.string == 'return':
      next_token = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES)
      if not next_token.IsType(Type.SEMICOLON):
        function = self.GetFunction()
        if function:
          function.has_return = True

    elif type == Type.KEYWORD and token.string == 'throw':
      function = self.GetFunction()
      if function:
        function.has_throw = True

    elif type == Type.SIMPLE_LVALUE:
      identifier = token.values['identifier']
      jsdoc = self.GetDocComment()
      if jsdoc:
        self._documented_identifiers.add(identifier)

      self._HandleIdentifier(identifier, True)

    elif type == Type.IDENTIFIER:
      self._HandleIdentifier(token.string, False)

      # Detect documented non-assignments.
      next_token = tokenutil.SearchExcept(token, Type.NON_CODE_TYPES)
      if next_token.IsType(Type.SEMICOLON):
        if (self._last_non_space_token and
            self._last_non_space_token.IsType(Type.END_DOC_COMMENT)):
          self._documented_identifiers.add(token.string)

  def _HandleIdentifier(self, identifier, is_assignment):
    """Process the given identifier.

    Currently checks if it references 'this' and annotates the function
    accordingly.

    Args:
      identifier: The identifer to process.
      is_assignment: Whether the identifer is being written to.
    """
    if identifier == 'this' or identifier.startswith('this.'):
      function = self.GetFunction()
      if function:
        function.has_this = True


  def HandleAfterToken(self, token):
    """Handle updating state after a token has been checked.

    This function should be used for destructive state changes such as
    deleting a tracked object.

    Args:
      token: The token to handle.
    """
    type = token.type
    if type == Type.SEMICOLON or type == Type.END_PAREN or (
        type == Type.END_BRACKET and
        self._last_non_space_token.type not in (
            Type.SINGLE_QUOTE_STRING_END, Type.DOUBLE_QUOTE_STRING_END)):
      # We end on any numeric array index, but keep going for string based
      # array indices so that we pick up manually exported identifiers.
      self._doc_comment = None
      self._last_comment = None

    elif type == Type.END_BLOCK:
      self._doc_comment = None
      self._last_comment = None

      if self.InFunction() and self.IsFunctionClose():
        # TODO(robbyw): Detect the function's name for better errors.
        self._functions.pop()

    elif type == Type.END_PARAMETERS and self._doc_comment:
      self._doc_comment = None
      self._last_comment = None

    if not token.IsAnyType(Type.WHITESPACE, Type.BLANK_LINE):
      self._last_non_space_token = token

    self._last_line = token.line
