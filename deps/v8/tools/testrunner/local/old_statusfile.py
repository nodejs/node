# Copyright 2012 the V8 project authors. All rights reserved.
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


import cStringIO
import re

# These outcomes can occur in a TestCase's outcomes list:
SKIP = 'SKIP'
FAIL = 'FAIL'
PASS = 'PASS'
OKAY = 'OKAY'
TIMEOUT = 'TIMEOUT'
CRASH = 'CRASH'
SLOW = 'SLOW'
# These are just for the status files and are mapped below in DEFS:
FAIL_OK = 'FAIL_OK'
PASS_OR_FAIL = 'PASS_OR_FAIL'

KEYWORDS = {SKIP: SKIP,
            FAIL: FAIL,
            PASS: PASS,
            OKAY: OKAY,
            TIMEOUT: TIMEOUT,
            CRASH: CRASH,
            SLOW: SLOW,
            FAIL_OK: FAIL_OK,
            PASS_OR_FAIL: PASS_OR_FAIL}

class Expression(object):
  pass


class Constant(Expression):

  def __init__(self, value):
    self.value = value

  def Evaluate(self, env, defs):
    return self.value


class Variable(Expression):

  def __init__(self, name):
    self.name = name

  def GetOutcomes(self, env, defs):
    if self.name in env: return set([env[self.name]])
    else: return set([])

  def Evaluate(self, env, defs):
    return env[self.name]

  def __str__(self):
    return self.name

  def string(self, logical):
    return self.__str__()


class Outcome(Expression):

  def __init__(self, name):
    self.name = name

  def GetOutcomes(self, env, defs):
    if self.name in defs:
      return defs[self.name].GetOutcomes(env, defs)
    else:
      return set([self.name])

  def __str__(self):
    if self.name in KEYWORDS:
      return "%s" % KEYWORDS[self.name]
    return "'%s'" % self.name

  def string(self, logical):
    if logical:
      return "%s" % self.name
    return self.__str__()


class Operation(Expression):

  def __init__(self, left, op, right):
    self.left = left
    self.op = op
    self.right = right

  def Evaluate(self, env, defs):
    if self.op == '||' or self.op == ',':
      return self.left.Evaluate(env, defs) or self.right.Evaluate(env, defs)
    elif self.op == 'if':
      return False
    elif self.op == '==':
      return not self.left.GetOutcomes(env, defs).isdisjoint(self.right.GetOutcomes(env, defs))
    elif self.op == '!=':
      return self.left.GetOutcomes(env, defs).isdisjoint(self.right.GetOutcomes(env, defs))
    else:
      assert self.op == '&&'
      return self.left.Evaluate(env, defs) and self.right.Evaluate(env, defs)

  def GetOutcomes(self, env, defs):
    if self.op == '||' or self.op == ',':
      return self.left.GetOutcomes(env, defs) | self.right.GetOutcomes(env, defs)
    elif self.op == 'if':
      if self.right.Evaluate(env, defs): return self.left.GetOutcomes(env, defs)
      else: return set([])
    else:
      assert self.op == '&&'
      return self.left.GetOutcomes(env, defs) & self.right.GetOutcomes(env, defs)

  def __str__(self):
    return self.string(False)

  def string(self, logical=False):
    if self.op == 'if':
      return "['%s', %s]" % (self.right.string(True), self.left.string(logical))
    elif self.op == "||" or self.op == ",":
      if logical:
        return "%s or %s" % (self.left.string(True), self.right.string(True))
      else:
        return "%s, %s" % (self.left, self.right)
    elif self.op == "&&":
      return "%s and %s" % (self.left.string(True), self.right.string(True))
    return "%s %s %s" % (self.left.string(logical), self.op,
                         self.right.string(logical))


def IsAlpha(string):
  for char in string:
    if not (char.isalpha() or char.isdigit() or char == '_'):
      return False
  return True


class Tokenizer(object):
  """A simple string tokenizer that chops expressions into variables,
  parens and operators"""

  def __init__(self, expr):
    self.index = 0
    self.expr = expr
    self.length = len(expr)
    self.tokens = None

  def Current(self, length=1):
    if not self.HasMore(length): return ""
    return self.expr[self.index:self.index + length]

  def HasMore(self, length=1):
    return self.index < self.length + (length - 1)

  def Advance(self, count=1):
    self.index = self.index + count

  def AddToken(self, token):
    self.tokens.append(token)

  def SkipSpaces(self):
    while self.HasMore() and self.Current().isspace():
      self.Advance()

  def Tokenize(self):
    self.tokens = [ ]
    while self.HasMore():
      self.SkipSpaces()
      if not self.HasMore():
        return None
      if self.Current() == '(':
        self.AddToken('(')
        self.Advance()
      elif self.Current() == ')':
        self.AddToken(')')
        self.Advance()
      elif self.Current() == '$':
        self.AddToken('$')
        self.Advance()
      elif self.Current() == ',':
        self.AddToken(',')
        self.Advance()
      elif IsAlpha(self.Current()):
        buf = ""
        while self.HasMore() and IsAlpha(self.Current()):
          buf += self.Current()
          self.Advance()
        self.AddToken(buf)
      elif self.Current(2) == '&&':
        self.AddToken('&&')
        self.Advance(2)
      elif self.Current(2) == '||':
        self.AddToken('||')
        self.Advance(2)
      elif self.Current(2) == '==':
        self.AddToken('==')
        self.Advance(2)
      elif self.Current(2) == '!=':
        self.AddToken('!=')
        self.Advance(2)
      else:
        return None
    return self.tokens


class Scanner(object):
  """A simple scanner that can serve out tokens from a given list"""

  def __init__(self, tokens):
    self.tokens = tokens
    self.length = len(tokens)
    self.index = 0

  def HasMore(self):
    return self.index < self.length

  def Current(self):
    return self.tokens[self.index]

  def Advance(self):
    self.index = self.index + 1


def ParseAtomicExpression(scan):
  if scan.Current() == "true":
    scan.Advance()
    return Constant(True)
  elif scan.Current() == "false":
    scan.Advance()
    return Constant(False)
  elif IsAlpha(scan.Current()):
    name = scan.Current()
    scan.Advance()
    return Outcome(name)
  elif scan.Current() == '$':
    scan.Advance()
    if not IsAlpha(scan.Current()):
      return None
    name = scan.Current()
    scan.Advance()
    return Variable(name.lower())
  elif scan.Current() == '(':
    scan.Advance()
    result = ParseLogicalExpression(scan)
    if (not result) or (scan.Current() != ')'):
      return None
    scan.Advance()
    return result
  else:
    return None


BINARIES = ['==', '!=']
def ParseOperatorExpression(scan):
  left = ParseAtomicExpression(scan)
  if not left: return None
  while scan.HasMore() and (scan.Current() in BINARIES):
    op = scan.Current()
    scan.Advance()
    right = ParseOperatorExpression(scan)
    if not right:
      return None
    left = Operation(left, op, right)
  return left


def ParseConditionalExpression(scan):
  left = ParseOperatorExpression(scan)
  if not left: return None
  while scan.HasMore() and (scan.Current() == 'if'):
    scan.Advance()
    right = ParseOperatorExpression(scan)
    if not right:
      return None
    left = Operation(left, 'if', right)
  return left


LOGICALS = ["&&", "||", ","]
def ParseLogicalExpression(scan):
  left = ParseConditionalExpression(scan)
  if not left: return None
  while scan.HasMore() and (scan.Current() in LOGICALS):
    op = scan.Current()
    scan.Advance()
    right = ParseConditionalExpression(scan)
    if not right:
      return None
    left = Operation(left, op, right)
  return left


def ParseCondition(expr):
  """Parses a logical expression into an Expression object"""
  tokens = Tokenizer(expr).Tokenize()
  if not tokens:
    print "Malformed expression: '%s'" % expr
    return None
  scan = Scanner(tokens)
  ast = ParseLogicalExpression(scan)
  if not ast:
    print "Malformed expression: '%s'" % expr
    return None
  if scan.HasMore():
    print "Malformed expression: '%s'" % expr
    return None
  return ast


class Section(object):
  """A section of the configuration file.  Sections are enabled or
  disabled prior to running the tests, based on their conditions"""

  def __init__(self, condition):
    self.condition = condition
    self.rules = [ ]

  def AddRule(self, rule):
    self.rules.append(rule)


class Rule(object):
  """A single rule that specifies the expected outcome for a single
  test."""

  def __init__(self, raw_path, path, value):
    self.raw_path = raw_path
    self.path = path
    self.value = value

  def GetOutcomes(self, env, defs):
    return self.value.GetOutcomes(env, defs)

  def Contains(self, path):
    if len(self.path) > len(path):
      return False
    for i in xrange(len(self.path)):
      if not self.path[i].match(path[i]):
        return False
    return True


HEADER_PATTERN = re.compile(r'\[([^]]+)\]')
RULE_PATTERN = re.compile(r'\s*([^: ]*)\s*:(.*)')
DEF_PATTERN = re.compile(r'^def\s*(\w+)\s*=(.*)$')
PREFIX_PATTERN = re.compile(r'^\s*prefix\s+([\w\_\.\-\/]+)$')


class ConvertNotation(object):
  def __init__(self, path):
    self.path = path
    self.indent = ""
    self.comment = []
    self.init = False
    self.section = False
    self.out = cStringIO.StringIO()

  def OpenGlobal(self):
    if self.init: return
    self.WriteComment()
    print >> self.out, "["
    self.init = True

  def CloseGlobal(self):
    if not self.init: return
    print >> self.out, "]"
    self.init = False

  def OpenSection(self, condition="ALWAYS"):
    if self.section: return
    self.OpenGlobal()
    if type(condition) != str:
      condition = "'%s'" % condition.string(True)
    print >> self.out, "%s[%s, {" % (self.indent, condition)
    self.indent += " " * 2
    self.section = condition

  def CloseSection(self):
    if not self.section: return
    self.indent = self.indent[:-2]
    print >> self.out, "%s}],  # %s" % (self.indent, self.section)
    self.section = False

  def WriteComment(self):
    if not self.comment: return
    for c in self.comment:
      if len(c.strip()) == 0:
        print >> self.out, ""
      else:
        print >> self.out, "%s%s" % (self.indent, c),
    self.comment = []

  def GetOutput(self):
    with open(self.path) as f:
      for line in f:
        if line[0] == '#':
          self.comment += [line]
          continue
        if len(line.strip()) == 0:
          self.comment += [line]
          continue
        header_match = HEADER_PATTERN.match(line)
        if header_match:
          condition = ParseCondition(header_match.group(1).strip())
          self.CloseSection()
          self.WriteComment()
          self.OpenSection(condition)
          continue
        rule_match = RULE_PATTERN.match(line)
        if rule_match:
          self.OpenSection()
          self.WriteComment()
          path = rule_match.group(1).strip()
          value_str = rule_match.group(2).strip()
          comment = ""
          if '#' in value_str:
            pos = value_str.find('#')
            comment = "  %s" % value_str[pos:].strip()
            value_str = value_str[:pos].strip()
          value = ParseCondition(value_str)
          print >> self.out, ("%s'%s': [%s],%s" %
                              (self.indent, path, value, comment))
          continue
        def_match = DEF_PATTERN.match(line)
        if def_match:
          # Custom definitions are deprecated.
          continue
        prefix_match = PREFIX_PATTERN.match(line)
        if prefix_match:
          continue
        print "Malformed line: '%s'." % line
    self.CloseSection()
    self.CloseGlobal()
    result = self.out.getvalue()
    self.out.close()
    return result
