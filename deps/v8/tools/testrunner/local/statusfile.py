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

# These outcomes can occur in a TestCase's outcomes list:
SKIP = "SKIP"
FAIL = "FAIL"
PASS = "PASS"
OKAY = "OKAY"
TIMEOUT = "TIMEOUT"
CRASH = "CRASH"
SLOW = "SLOW"
FLAKY = "FLAKY"
FAST_VARIANTS = "FAST_VARIANTS"
NO_VARIANTS = "NO_VARIANTS"
# These are just for the status files and are mapped below in DEFS:
FAIL_OK = "FAIL_OK"
PASS_OR_FAIL = "PASS_OR_FAIL"
FAIL_SLOPPY = "FAIL_SLOPPY"

ALWAYS = "ALWAYS"

KEYWORDS = {}
for key in [SKIP, FAIL, PASS, OKAY, TIMEOUT, CRASH, SLOW, FLAKY, FAIL_OK,
            FAST_VARIANTS, NO_VARIANTS, PASS_OR_FAIL, FAIL_SLOPPY, ALWAYS]:
  KEYWORDS[key] = key

DEFS = {FAIL_OK: [FAIL, OKAY],
        PASS_OR_FAIL: [PASS, FAIL]}

# Support arches, modes to be written as keywords instead of strings.
VARIABLES = {ALWAYS: True}
for var in ["debug", "release", "big", "little",
            "android_arm", "android_arm64", "android_ia32", "android_x87",
            "android_x64", "arm", "arm64", "ia32", "mips", "mipsel", "mips64",
            "mips64el", "x64", "x87", "nacl_ia32", "nacl_x64", "ppc", "ppc64",
            "macos", "windows", "linux", "aix"]:
  VARIABLES[var] = var


def DoSkip(outcomes):
  return SKIP in outcomes


def IsSlow(outcomes):
  return SLOW in outcomes


def OnlyStandardVariant(outcomes):
  return NO_VARIANTS in outcomes


def OnlyFastVariants(outcomes):
  return FAST_VARIANTS in outcomes


def IsFlaky(outcomes):
  return FLAKY in outcomes


def IsPassOrFail(outcomes):
  return ((PASS in outcomes) and (FAIL in outcomes) and
          (not CRASH in outcomes) and (not OKAY in outcomes))


def IsFailOk(outcomes):
    return (FAIL in outcomes) and (OKAY in outcomes)


def _AddOutcome(result, new):
  global DEFS
  if new in DEFS:
    mapped = DEFS[new]
    if type(mapped) == list:
      for m in mapped:
        _AddOutcome(result, m)
    elif type(mapped) == str:
      _AddOutcome(result, mapped)
  else:
    result.add(new)


def _ParseOutcomeList(rule, outcomes, target_dict, variables):
  result = set([])
  if type(outcomes) == str:
    outcomes = [outcomes]
  for item in outcomes:
    if type(item) == str:
      _AddOutcome(result, item)
    elif type(item) == list:
      if not eval(item[0], variables): continue
      for outcome in item[1:]:
        assert type(outcome) == str
        _AddOutcome(result, outcome)
    else:
      assert False
  if len(result) == 0: return
  if rule in target_dict:
    target_dict[rule] |= result
  else:
    target_dict[rule] = result


def ReadContent(path):
  with open(path) as f:
    global KEYWORDS
    return eval(f.read(), KEYWORDS)


def ReadStatusFile(path, variables):
  contents = ReadContent(path)

  rules = {}
  wildcards = {}
  variables.update(VARIABLES)
  for section in contents:
    assert type(section) == list
    assert len(section) == 2
    if not eval(section[0], variables): continue
    section = section[1]
    assert type(section) == dict
    for rule in section:
      assert type(rule) == str
      if rule[-1] == '*':
        _ParseOutcomeList(rule, section[rule], wildcards, variables)
      else:
        _ParseOutcomeList(rule, section[rule], rules, variables)
  return rules, wildcards


def PresubmitCheck(path):
  contents = ReadContent(path)
  root_prefix = os.path.basename(os.path.dirname(path)) + "/"
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
    return status["success"]
  except Exception as e:
    print e
    return False
