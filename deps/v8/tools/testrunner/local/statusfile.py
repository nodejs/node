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

import os
import re

from variants import ALL_VARIANTS
from utils import Freeze

# Possible outcomes
FAIL = "FAIL"
PASS = "PASS"
TIMEOUT = "TIMEOUT" # TODO(majeski): unused in status files
CRASH = "CRASH" # TODO(majeski): unused in status files

# Outcomes only for status file, need special handling
FAIL_OK = "FAIL_OK"
FAIL_SLOPPY = "FAIL_SLOPPY"

# Modifiers
SKIP = "SKIP"
SLOW = "SLOW"
FAST_VARIANTS = "FAST_VARIANTS"
NO_VARIANTS = "NO_VARIANTS"

ALWAYS = "ALWAYS"

KEYWORDS = {}
for key in [SKIP, FAIL, PASS, CRASH, SLOW, FAIL_OK, FAST_VARIANTS, NO_VARIANTS,
            FAIL_SLOPPY, ALWAYS]:
  KEYWORDS[key] = key

# Support arches, modes to be written as keywords instead of strings.
VARIABLES = {ALWAYS: True}
for var in ["debug", "release", "big", "little",
            "android_arm", "android_arm64", "android_ia32", "android_x64",
            "arm", "arm64", "ia32", "mips", "mipsel", "mips64", "mips64el",
            "x64", "ppc", "ppc64", "s390", "s390x", "macos", "windows",
            "linux", "aix"]:
  VARIABLES[var] = var

# Allow using variants as keywords.
for var in ALL_VARIANTS:
  VARIABLES[var] = var


def DoSkip(outcomes):
  return SKIP in outcomes


def IsSlow(outcomes):
  return SLOW in outcomes


def OnlyStandardVariant(outcomes):
  return NO_VARIANTS in outcomes


def OnlyFastVariants(outcomes):
  return FAST_VARIANTS in outcomes


def IsPassOrFail(outcomes):
  return (PASS in outcomes and
          FAIL in outcomes and
          CRASH not in outcomes)


def IsFailOk(outcomes):
  return FAIL_OK in outcomes


def _JoinsPassAndFail(outcomes1, outcomes2):
  """Indicates if we join PASS and FAIL from two different outcome sets and
  the first doesn't already contain both.
  """
  return (
      PASS in outcomes1 and
      not (FAIL in outcomes1 or FAIL_OK in outcomes1) and
      (FAIL in outcomes2 or FAIL_OK in outcomes2)
  )

VARIANT_EXPRESSION = object()

def _EvalExpression(exp, variables):
  """Evaluates expression and returns its result. In case of NameError caused by
  undefined "variant" identifier returns VARIANT_EXPRESSION marker.
  """

  try:
    return eval(exp, variables)
  except NameError as e:
    identifier = re.match("name '(.*)' is not defined", e.message).group(1)
    assert identifier == "variant", "Unknown identifier: %s" % identifier
    return VARIANT_EXPRESSION


def _EvalVariantExpression(
  condition, section, variables, variant, rules, prefix_rules):
  variables_with_variant = dict(variables)
  variables_with_variant["variant"] = variant
  result = _EvalExpression(condition, variables_with_variant)
  assert result != VARIANT_EXPRESSION
  if result is True:
    _ReadSection(
        section,
        variables_with_variant,
        rules[variant],
        prefix_rules[variant],
    )
  else:
    assert result is False, "Make sure expressions evaluate to boolean values"


def _ParseOutcomeList(rule, outcomes, variables, target_dict):
  """Outcome list format: [condition, outcome, outcome, ...]"""

  result = set([])
  if type(outcomes) == str:
    outcomes = [outcomes]
  for item in outcomes:
    if type(item) == str:
      result.add(item)
    elif type(item) == list:
      condition = item[0]
      exp = _EvalExpression(condition, variables)
      assert exp != VARIANT_EXPRESSION, (
        "Nested variant expressions are not supported")
      if exp is False:
        continue

      # Ensure nobody uses an identifier by mistake, like "default",
      # which would evaluate to true here otherwise.
      assert exp is True, "Make sure expressions evaluate to boolean values"

      for outcome in item[1:]:
        assert type(outcome) == str
        result.add(outcome)
    else:
      assert False
  if len(result) == 0:
    return
  if rule in target_dict:
    # A FAIL without PASS in one rule has always precedence over a single
    # PASS (without FAIL) in another. Otherwise the default PASS expectation
    # in a rule with a modifier (e.g. PASS, SLOW) would be joined to a FAIL
    # from another rule (which intended to mark a test as FAIL and not as
    # PASS and FAIL).
    if _JoinsPassAndFail(target_dict[rule], result):
      target_dict[rule] -= set([PASS])
    if _JoinsPassAndFail(result, target_dict[rule]):
      result -= set([PASS])
    target_dict[rule] |= result
  else:
    target_dict[rule] = result


def ReadContent(content):
  return eval(content, KEYWORDS)


def ReadStatusFile(content, variables):
  """Status file format
  Status file := [section]
  section = [CONDITION, section_rules]
  section_rules := {path: outcomes}
  outcomes := outcome | [outcome, ...]
  outcome := SINGLE_OUTCOME | [CONDITION, SINGLE_OUTCOME, SINGLE_OUTCOME, ...]
  """

  # Empty defaults for rules and prefix_rules. Variant-independent
  # rules are mapped by "", others by the variant name.
  rules = {variant: {} for variant in ALL_VARIANTS}
  rules[""] = {}
  prefix_rules = {variant: {} for variant in ALL_VARIANTS}
  prefix_rules[""] = {}

  variables.update(VARIABLES)
  for conditional_section in ReadContent(content):
    assert type(conditional_section) == list
    assert len(conditional_section) == 2
    condition, section = conditional_section
    exp = _EvalExpression(condition, variables)

    # The expression is variant-independent and evaluates to False.
    if exp is False:
      continue

    # The expression is variant-independent and evaluates to True.
    if exp is True:
      _ReadSection(
          section,
          variables,
          rules[''],
          prefix_rules[''],
      )
      continue

    # The expression is variant-dependent (contains "variant" keyword)
    if exp == VARIANT_EXPRESSION:
      # If the expression contains one or more "variant" keywords, we evaluate
      # it for all possible variants and create rules for those that apply.
      for variant in ALL_VARIANTS:
        _EvalVariantExpression(
            condition, section, variables, variant, rules, prefix_rules)
      continue

    assert False, "Make sure expressions evaluate to boolean values"

  return Freeze(rules), Freeze(prefix_rules)


def _ReadSection(section, variables, rules, prefix_rules):
  assert type(section) == dict
  for rule, outcome_list in section.iteritems():
    assert type(rule) == str

    if rule[-1] == '*':
      _ParseOutcomeList(rule[:-1], outcome_list, variables, prefix_rules)
    else:
      _ParseOutcomeList(rule, outcome_list, variables, rules)

JS_TEST_PATHS = {
  'debugger': [[]],
  'inspector': [[]],
  'intl': [[]],
  'message': [[]],
  'mjsunit': [[]],
  'mozilla': [['data']],
  'test262': [['data', 'test'], ['local-tests', 'test']],
  'webkit': [[]],
}

def PresubmitCheck(path):
  with open(path) as f:
    contents = ReadContent(f.read())
  basename = os.path.basename(os.path.dirname(path))
  root_prefix = basename + "/"
  status = {"success": True}
  def _assert(check, message):  # Like "assert", but doesn't throw.
    if not check:
      print("%s: Error: %s" % (path, message))
      status["success"] = False
  try:
    for section in contents:
      _assert(type(section) == list, "Section must be a list")
      _assert(len(section) == 2, "Section list must have exactly 2 entries")
      section = section[1]
      _assert(type(section) == dict,
              "Second entry of section must be a dictionary")
      for rule in section:
        _assert(type(rule) == str, "Rule key must be a string")
        _assert(not rule.startswith(root_prefix),
                "Suite name prefix must not be used in rule keys")
        _assert(not rule.endswith('.js'),
                ".js extension must not be used in rule keys.")
        _assert('*' not in rule or (rule.count('*') == 1 and rule[-1] == '*'),
                "Only the last character of a rule key can be a wildcard")
        if basename in JS_TEST_PATHS  and '*' not in rule:
          _assert(any(os.path.exists(os.path.join(os.path.dirname(path),
                                                  *(paths + [rule + ".js"])))
                      for paths in JS_TEST_PATHS[basename]),
                  "missing file for %s test %s" % (basename, rule))
    return status["success"]
  except Exception as e:
    print e
    return False
