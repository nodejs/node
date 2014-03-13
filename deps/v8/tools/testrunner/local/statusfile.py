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


# These outcomes can occur in a TestCase's outcomes list:
SKIP = "SKIP"
FAIL = "FAIL"
PASS = "PASS"
OKAY = "OKAY"
TIMEOUT = "TIMEOUT"
CRASH = "CRASH"
SLOW = "SLOW"
FLAKY = "FLAKY"
NO_VARIANTS = "NO_VARIANTS"
# These are just for the status files and are mapped below in DEFS:
FAIL_OK = "FAIL_OK"
PASS_OR_FAIL = "PASS_OR_FAIL"

ALWAYS = "ALWAYS"

KEYWORDS = {}
for key in [SKIP, FAIL, PASS, OKAY, TIMEOUT, CRASH, SLOW, FLAKY, FAIL_OK,
            NO_VARIANTS, PASS_OR_FAIL, ALWAYS]:
  KEYWORDS[key] = key

DEFS = {FAIL_OK: [FAIL, OKAY],
        PASS_OR_FAIL: [PASS, FAIL]}

# Support arches, modes to be written as keywords instead of strings.
VARIABLES = {ALWAYS: True}
for var in ["debug", "release", "android_arm", "android_ia32", "arm", "a64",
            "ia32", "mipsel", "x64", "nacl_ia32", "nacl_x64", "macos",
            "windows", "linux"]:
  VARIABLES[var] = var


def DoSkip(outcomes):
  return SKIP in outcomes


def IsSlow(outcomes):
  return SLOW in outcomes


def OnlyStandardVariant(outcomes):
  return NO_VARIANTS in outcomes


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


def ReadStatusFile(path, variables):
  with open(path) as f:
    global KEYWORDS
    contents = eval(f.read(), KEYWORDS)

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
