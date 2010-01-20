#!/usr/bin/python2.4

# Copyright 2009 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""A JavaScript minifier.

It is far from being a complete JS parser, so there are many valid
JavaScript programs that will be ruined by it.  Another strangeness is that
it accepts $ and % as parts of identifiers.  It doesn't merge lines or strip
out blank lines in order to ease debugging.  Variables at the top scope are
properties of the global object so we can't rename them.  It is assumed that
you introduce variables with var as if JavaScript followed C++ scope rules
around curly braces, so the declaration must be above the first use.

Use as:
import jsmin
minifier = JavaScriptMinifier()
program1 = minifier.JSMinify(program1)
program2 = minifier.JSMinify(program2)
"""

import re


class JavaScriptMinifier(object):
  """An object that you can feed code snippets to to get them minified."""

  def __init__(self):
    # We prepopulate the list of identifiers that shouldn't be used.  These
    # short language keywords could otherwise be used by the script as variable
    # names.
    self.seen_identifiers = {"do": True, "in": True}
    self.identifier_counter = 0
    self.in_comment = False
    self.map = {}
    self.nesting = 0

  def LookAtIdentifier(self, m):
    """Records identifiers or keywords that we see in use.

    (So we can avoid renaming variables to these strings.)
    Args:
      m: The match object returned by re.search.

    Returns:
      Nothing.
    """
    identifier = m.group(1)
    self.seen_identifiers[identifier] = True

  def Push(self):
    """Called when we encounter a '{'."""
    self.nesting += 1

  def Pop(self):
    """Called when we encounter a '}'."""
    self.nesting -= 1
    # We treat each top-level opening brace as a single scope that can span
    # several sets of nested braces.
    if self.nesting == 0:
      self.map = {}
      self.identifier_counter = 0

  def Declaration(self, m):
    """Rewrites bits of the program selected by a regexp.

    These can be curly braces, literal strings, function declarations and var
    declarations.  (These last two must be on one line including the opening
    curly brace of the function for their variables to be renamed).

    Args:
      m: The match object returned by re.search.

    Returns:
      The string that should replace the match in the rewritten program.
    """
    matched_text = m.group(0)
    if matched_text == "{":
      self.Push()
      return matched_text
    if matched_text == "}":
      self.Pop()
      return matched_text
    if re.match("[\"'/]", matched_text):
      return matched_text
    m = re.match(r"var ", matched_text)
    if m:
      var_names = matched_text[m.end():]
      var_names = re.split(r",", var_names)
      return "var " + ",".join(map(self.FindNewName, var_names))
    m = re.match(r"(function\b[^(]*)\((.*)\)\{$", matched_text)
    if m:
      up_to_args = m.group(1)
      args = m.group(2)
      args = re.split(r",", args)
      self.Push()
      return up_to_args + "(" + ",".join(map(self.FindNewName, args)) + "){"

    if matched_text in self.map:
      return self.map[matched_text]

    return matched_text

  def CharFromNumber(self, number):
    """A single-digit base-52 encoding using a-zA-Z."""
    if number < 26:
      return chr(number + 97)
    number -= 26
    return chr(number + 65)

  def FindNewName(self, var_name):
    """Finds a new 1-character or 2-character name for a variable.

    Enters it into the mapping table for this scope.

    Args:
      var_name: The name of the variable before renaming.

    Returns:
      The new name of the variable.
    """
    new_identifier = ""
    # Variable names that end in _ are member variables of the global object,
    # so they can be visible from code in a different scope.  We leave them
    # alone.
    if var_name in self.map:
      return self.map[var_name]
    if self.nesting == 0:
      return var_name
    while True:
      identifier_first_char = self.identifier_counter % 52
      identifier_second_char = self.identifier_counter / 52
      new_identifier = self.CharFromNumber(identifier_first_char)
      if identifier_second_char != 0:
        new_identifier = (
            self.CharFromNumber(identifier_second_char - 1) + new_identifier)
      self.identifier_counter += 1
      if not new_identifier in self.seen_identifiers:
        break

    self.map[var_name] = new_identifier
    return new_identifier

  def RemoveSpaces(self, m):
    """Returns literal strings unchanged, replaces other inputs with group 2.

    Other inputs are replaced with the contents of capture 1.  This is either
    a single space or an empty string.

    Args:
      m: The match object returned by re.search.

    Returns:
      The string that should be inserted instead of the matched text.
    """
    entire_match = m.group(0)
    replacement = m.group(1)
    if re.match(r"'.*'$", entire_match):
      return entire_match
    if re.match(r'".*"$', entire_match):
      return entire_match
    if re.match(r"/.+/$", entire_match):
      return entire_match
    return replacement

  def JSMinify(self, text):
    """The main entry point.  Takes a text and returns a compressed version.

    The compressed version hopefully does the same thing.  Line breaks are
    preserved.

    Args:
      text: The text of the code snippet as a multiline string.

    Returns:
      The compressed text of the code snippet as a multiline string.
    """
    new_lines = []
    for line in re.split(r"\n", text):
      line = line.replace("\t", " ")
      if self.in_comment:
        m = re.search(r"\*/", line)
        if m:
          line = line[m.end():]
          self.in_comment = False
        else:
          new_lines.append("")
          continue

      if not self.in_comment:
        line = re.sub(r"/\*.*?\*/", " ", line)
        line = re.sub(r"//.*", "", line)
        m = re.search(r"/\*", line)
        if m:
          line = line[:m.start()]
          self.in_comment = True

      # Strip leading and trailing spaces.
      line = re.sub(r"^ +", "", line)
      line = re.sub(r" +$", "", line)
      # A regexp that matches a literal string surrounded by "double quotes".
      # This regexp can handle embedded backslash-escaped characters including
      # embedded backslash-escaped double quotes.
      double_quoted_string = r'"(?:[^"\\]|\\.)*"'
      # A regexp that matches a literal string surrounded by 'double quotes'.
      single_quoted_string = r"'(?:[^'\\]|\\.)*'"
      # A regexp that matches a regexp literal surrounded by /slashes/.
      # Don't allow a regexp to have a ) before the first ( since that's a
      # syntax error and it's probably just two unrelated slashes.
      slash_quoted_regexp = r"/(?:(?=\()|(?:[^()/\\]|\\.)+)(?:\([^/\\]|\\.)*/"
      # Replace multiple spaces with a single space.
      line = re.sub("|".join([double_quoted_string,
                              single_quoted_string,
                              slash_quoted_regexp,
                              "( )+"]),
                    self.RemoveSpaces,
                    line)
      # Strip single spaces unless they have an identifier character both before
      # and after the space.  % and $ are counted as identifier characters.
      line = re.sub("|".join([double_quoted_string,
                              single_quoted_string,
                              slash_quoted_regexp,
                              r"(?<![a-zA-Z_0-9$%]) | (?![a-zA-Z_0-9$%])()"]),
                    self.RemoveSpaces,
                    line)
      # Collect keywords and identifiers that are already in use.
      if self.nesting == 0:
        re.sub(r"([a-zA-Z0-9_$%]+)", self.LookAtIdentifier, line)
      function_declaration_regexp = (
          r"\bfunction"              # Function definition keyword...
          r"( [\w$%]+)?"             # ...optional function name...
          r"\([\w$%,]+\)\{")         # ...argument declarations.
      # Unfortunately the keyword-value syntax { key:value } makes the key look
      # like a variable where in fact it is a literal string.  We use the
      # presence or absence of a question mark to try to distinguish between
      # this case and the ternary operator: "condition ? iftrue : iffalse".
      if re.search(r"\?", line):
        block_trailing_colon = r""
      else:
        block_trailing_colon = r"(?![:\w$%])"
      # Variable use.  Cannot follow a period precede a colon.
      variable_use_regexp = r"(?<![.\w$%])[\w$%]+" + block_trailing_colon
      line = re.sub("|".join([double_quoted_string,
                              single_quoted_string,
                              slash_quoted_regexp,
                              r"\{",                  # Curly braces.
                              r"\}",
                              r"\bvar [\w$%,]+",      # var declarations.
                              function_declaration_regexp,
                              variable_use_regexp]),
                    self.Declaration,
                    line)
      new_lines.append(line)

    return "\n".join(new_lines) + "\n"
