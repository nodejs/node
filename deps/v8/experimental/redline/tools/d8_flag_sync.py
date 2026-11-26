#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compares the random d8 flags of Redline with V8 and Fuzzilli sources.

Reads the flag names and probabilities from `redline fuzz --help-tunables` (or
from `kFuzzilliFlagTable` in `engine/d8_flags.cc` when `--profile=fuzzilli` or
`--profile=fuzzilli-wasm` is given) and compares them with
`tools/clusterfuzz/trials/clusterfuzz_trials_config.json`,
`tools/clusterfuzz/foozzie/v8_fuzz_flags.json`, and (when `--fuzzilli=PATH` is
given) `Sources/Fuzzilli/Profiles/V8CommonProfile.swift`.

Reports three groups:
  1. Flags that a source draws and Redline does not draw, with notes when `d8`
     already has the flag on (`--d8`), when the flag is an alias of a Redline
     flag, or when a Redline flag implies it.
  2. Flags where Redline and a source have different probabilities.
  3. Flags that Redline draws and no source has.

Known limits of static `flag-definitions.h` parsing:
  - Conditional `#if` / `#ifdef` blocks and build-configured macro defaults
    (such as `ENABLE_SPARKPLUG_BY_DEFAULT` or `V8_ENABLE_TURBOFAN_BOOL`) are
    not evaluated. Unambiguous single-value `#define` booleans are resolved,
    and `DEBUG_BOOL` defaults to `false` unless `--debug-bool` is passed. Pass
    `--d8` to inspect active flags on a real build.
  - Implications expanded inside preprocessor macro lists (such as `HARMONY_*`
    or `WASM_STAGING_IMPLICATION`) are not seen.
  - Compound C++ expressions in `DEFINE_*IMPLICATION` and arbitrary statements
    in `DEFINE_GENERIC_IMPLICATION` are not parsed.

With `--check-d8=D8`, runs `D8 --help` once and reports each flag name of
`kFuzzilliFlagTable` and of `RandomD8Flags` that the `d8` binary does not have.

Exits with status 0 by default, or 1 when `--strict` is set and a difference
exists (or when `--check-draws` finds a frequency difference above tolerance,
or when `--check-d8` finds an unknown flag name).
"""

import argparse
import collections
import json
import math
import os
import re
import subprocess
import sys

V8_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
DEFAULT_REDLINE = os.path.join(V8_ROOT, 'out', 'redline', 'redline')
FUZZILLI_PROFILE_REL = os.path.join('Sources', 'Fuzzilli', 'Profiles',
                                    'V8CommonProfile.swift')
DEFAULT_BASE_D8_ARGS = (
    '--expose-gc',
    '--expose-externalize-string',
    '--omit-quit',
    '--allow-natives-syntax',
    '--fuzzing',
    '--harmony',
    '--experimental-fuzzing',
    '--js-staging',
    '--wasm-staging',
    '--wasm-fast-api',
    '--expose-fast-api',
    '--wasm-test-streaming',
)

TUNABLE_LINE = re.compile(r'^\s+(flags\.\S+)\s*=\s*([0-9.e+-]+)\s+'
                          r'\(default\s+[0-9.e+-]+,\s+range\s+0\.\.1\)\s+(.*)$')
DESC_FLAG = re.compile(
    r'(?:probability of|together with)\s+(--[a-z0-9-]+(?:=[^,\s:]+)?)')
DEFINE_MACRO_BOOL = re.compile(
    r'^\s*#\s*define\s+([A-Z][A-Z0-9_]*)\s+(true|false)\s*(?://.*|/\*.*\*/)?$',
    re.MULTILINE)
DEFINE_BOOL = re.compile(r'\b(DEFINE_(?:BOOL|BOOL_READONLY|DEBUG_BOOL))\(\s*'
                         r'([a-z0-9_]+)\s*,\s*(!?[A-Za-z0-9_]+)\b')
DEFINE_FALSE_BOOL_FLAG = re.compile(
    r'\bDEFINE_(?:EXPERIMENTAL_FEATURE|UNSAFE_FLAG|DEVELOPER_FLAG|'
    r'DEBUG_DEVELOPER_FLAG|VERIFICATION_FLAG|PERF_PROF_BOOL|'
    r'SLOW_TRACING_BOOL)\(\s*([a-z0-9_]+)\s*,')
DEFINE_IMPLICATION = re.compile(
    r'\b(DEFINE_(?:WEAK_)?(?:NEG_NEG_|NEG_VALUE_|NEG_|DISABLE_FLAG_|VALUE_)?'
    r'IMPLICATION(?:_WITH_WARNING)?)\(\s*(!?[a-z0-9_]+)\s*,\s*([a-z0-9_]+)'
    r'(?:\s*,\s*([^)]+))?\)')
DEFINE_ALIAS = re.compile(
    r'\bDEFINE_ALIAS_[A-Z_]+\(\s*([a-z0-9_]+)\s*,\s*([a-z0-9_]+)')
DUMMY_FLAG_PARAMS = frozenset(
    ('nam', 'name', 'alias', 'cond', 'whenflag', 'thenflag'))
TABLE_ENTRY = re.compile(
    r'\{\s*FuzzilliCond::(k\w+)\s*,\s*FuzzilliDrawKind::(k\w+)\s*,\s*'
    r'([0-9.e+-]+)\s*,\s*"((?:--[a-z0-9-]+=?\d*)?)"\s*,\s*(-?\d+)\s*,\s*(-?\d+)'
    r'(?:\s*,\s*(true|false))?\s*\}')
BOOL_HELPER_CALL = re.compile(
    r'chooseBooleanFlag(?:Randomly)?\("([a-z0-9-]+)"\)')
SWIFT_CONTAINS = re.compile(r'args\.contains\("(--[a-z0-9_-]+)(?:=[^"]*)?"\)')
# The values of the `config.*` fields of `v8ProcessArgs` for the non-Wasm
# comparison. Wasm-only fields stay false.
SWIFT_CONFIG = {
    'randomize': True,
    'isWasmEnabled': False,
    'enableCustomDescriptors': False
}
# The conditions of kFuzzilliFlagTable that draw a flag only if an earlier row
# did not draw the same flag name. The two draws exclude each other.
EXCLUSIVE_CONDS = frozenset(('kIfTurbolevNoSmallZero',))
D8_HELP_FLAG = re.compile(r'^\s+(--[a-z0-9_-]+)\b', re.MULTILINE)
PROB_TOLERANCE = 1e-4
DEFAULT_MAX_SIGMA = 4.0
PRESET_PARENT_FLAGS = frozenset(
    ('--no-turbofan', '--turbolev', '--turbolev-future'))


def normalize_flag(raw):
  """Returns the canonical `--flag-name` without a `=value` suffix."""
  name = raw.split('=', 1)[0].strip()
  return name.replace('_', '-')


def _flip_polarity(flag):
  """Returns `--no-X` for `--X` and `--X` for `--no-X`."""
  if flag.startswith('--no-'):
    return '--' + flag[5:]
  if flag.startswith('--'):
    return '--no-' + flag[2:]
  return flag


def _expand_wasm_polarities(wasm_flags):
  """Returns `wasm_flags` together with the opposite boolean polarities."""
  expanded = set()
  for flag in wasm_flags:
    norm = normalize_flag(flag)
    expanded.add(norm)
    expanded.add(_flip_polarity(norm))
  return expanded


def _combine_indep(p_existing, p_new):
  """Returns `1 - (1 - p_existing) * (1 - p_new)`."""
  return 1.0 - (1.0 - p_existing) * (1.0 - p_new)


def _parse_unambiguous_bool_macros(text):
  """Returns `{MACRO_NAME: bool}` for `#define` booleans with one value."""
  seen = {}
  for match in DEFINE_MACRO_BOOL.finditer(text):
    macro_name, val_text = match.groups()
    val = val_text == 'true'
    if macro_name not in seen:
      seen[macro_name] = val
    elif seen[macro_name] != val:
      seen[macro_name] = None
  return {k: v for k, v in seen.items() if v is not None}


def parse_bool_defaults(text, debug_bool=False):
  """Returns `{flag_name_without_dashes: bool}` from `flag-definitions.h`."""
  macro_defs = _parse_unambiguous_bool_macros(text)
  macro_defs['DEBUG_BOOL'] = bool(debug_bool)

  result = {}
  for match in DEFINE_FALSE_BOOL_FLAG.finditer(text):
    raw_name = match.group(1)
    if raw_name in DUMMY_FLAG_PARAMS:
      continue
    result[raw_name.replace('_', '-')] = False

  for match in DEFINE_BOOL.finditer(text):
    macro, raw_name, raw_val = match.groups()
    if raw_name in DUMMY_FLAG_PARAMS or raw_val == 'def':
      continue
    negated = raw_val.startswith('!')
    token = raw_val[1:] if negated else raw_val
    if token == 'true':
      val = True
    elif token == 'false':
      val = False
    elif token in macro_defs:
      val = macro_defs[token]
    else:
      continue
    if negated:
      val = not val
    name = raw_name.replace('_', '-')
    if macro == 'DEFINE_BOOL_READONLY' and name in result:
      continue
    result[name] = val
  return result


def parse_flag_aliases(text):
  """Returns `{flag: [other_flag, ...]}` from `DEFINE_ALIAS_*` macros."""
  result = {}

  def add(flag, other):
    entries = result.setdefault(flag, [])
    if other not in entries:
      entries.append(other)

  for match in DEFINE_ALIAS.finditer(text):
    raw_alias, raw_target = match.groups()
    if raw_alias in DUMMY_FLAG_PARAMS or raw_target in DUMMY_FLAG_PARAMS:
      continue
    alias = '--' + raw_alias.replace('_', '-')
    target = '--' + raw_target.replace('_', '-')
    add(alias, target)
    add(target, alias)
    neg_alias = '--no-' + raw_alias.replace('_', '-')
    neg_target = '--no-' + raw_target.replace('_', '-')
    add(neg_alias, neg_target)
    add(neg_target, neg_alias)
  return result


def parse_flag_implications(text):
  """Returns `{target_flag: [(when_flag, implied_token), ...]}` from C++ defs."""
  result = {}

  def add(target_flag, when_flag, implied_token):
    entries = result.setdefault(target_flag, [])
    pair = (when_flag, implied_token)
    if pair not in entries:
      entries.append(pair)

  neg_premise_macros = frozenset(
      ('DEFINE_NEG_NEG_IMPLICATION', 'DEFINE_NEG_VALUE_IMPLICATION'))
  neg_target_macros = frozenset((
      'DEFINE_NEG_IMPLICATION',
      'DEFINE_NEG_IMPLICATION_WITH_WARNING',
      'DEFINE_WEAK_NEG_IMPLICATION',
      'DEFINE_NEG_NEG_IMPLICATION',
      'DEFINE_DISABLE_FLAG_IMPLICATION',
  ))

  for match in DEFINE_IMPLICATION.finditer(text):
    macro, when_raw, then_raw, val_raw = match.groups()
    when_clean = when_raw[1:] if when_raw.startswith('!') else when_raw
    if when_clean in DUMMY_FLAG_PARAMS or then_raw in DUMMY_FLAG_PARAMS:
      continue
    negate_when = macro in neg_premise_macros
    if when_raw.startswith('!'):
      negate_when = not negate_when
    when_flag = ('--no-' if negate_when else '--') + when_clean.replace(
        '_', '-')

    then_base = then_raw.replace('_', '-')
    if 'VALUE_' in macro:
      val = (val_raw or '').strip()
      if val == 'true':
        then_flag = '--' + then_base
        implied = then_flag
      elif val == 'false':
        then_flag = '--no-' + then_base
        implied = then_flag
      else:
        then_flag = '--' + then_base
        implied = f'--{then_base}={val}'
    elif macro in neg_target_macros:
      then_flag = '--no-' + then_base
      implied = then_flag
    else:
      then_flag = '--' + then_base
      implied = then_flag
    add(then_flag, when_flag, implied)
  return result


def is_default_on(flag, bool_defaults):
  """Returns True if `flag` is enabled by default in `bool_defaults`."""
  if not flag.startswith('--'):
    return None
  body = flag[2:]
  if body.startswith('no-'):
    name = body[3:]
    if name in bool_defaults:
      return bool_defaults[name] is False
    return None
  if body in bool_defaults:
    return bool_defaults[body] is True
  return None


def parse_d8_printed_flags(stdout_text):
  """Returns the set of active boolean flags (`--foo`, `--no-bar`) from `d8`."""
  active = set()
  for line in stdout_text.splitlines():
    stripped = line.strip()
    if not stripped.startswith('--'):
      continue
    if '=' not in stripped:
      active.add(stripped.replace('_', '-'))
    elif stripped.endswith(('=true', '=false')):
      name, val = stripped[2:].split('=', 1)
      canon = name.replace('_', '-')
      active.add(f'--{canon}' if val == 'true' else f'--no-{canon}')
  return active


def load_base_d8_args(redline_bin):
  """Returns Redline's default `d8_args`, or `DEFAULT_BASE_D8_ARGS`."""
  try:
    proc = subprocess.run([redline_bin, 'fuzz', '--dump-config'],
                          capture_output=True,
                          text=True,
                          check=True)
    return json.loads(proc.stdout).get('d8_args', list(DEFAULT_BASE_D8_ARGS))
  except (OSError, subprocess.CalledProcessError, json.JSONDecodeError):
    return list(DEFAULT_BASE_D8_ARGS)


def read_d8_active_flags(d8_bin, base_args):
  """Runs `d8 <base_args> --print-flag-values -e ""` and returns active flags."""
  env = dict(os.environ)
  env.pop('SHM_ID', None)
  cmd = [d8_bin] + list(base_args) + ['--print-flag-values', '-e', '']
  proc = subprocess.run(
      cmd, env=env, capture_output=True, text=True, check=True)
  return parse_d8_printed_flags(proc.stdout)


def parse_redline_tunables(text):
  """Returns `{flag: [probability, ...]}` from `--help-tunables` output."""
  result = {}
  for line in text.splitlines():
    match = TUNABLE_LINE.match(line)
    if not match:
      continue
    _, prob_text, desc = match.groups()
    if 'probability of --' not in desc:
      continue
    prob = float(prob_text)
    for flag_match in DESC_FLAG.finditer(desc):
      flag = normalize_flag(flag_match.group(1))
      probs = result.setdefault(flag, [])
      if not any(abs(existing - prob) <= PROB_TOLERANCE for existing in probs):
        probs.append(prob)
  return result


def read_redline_flags(redline_bin):
  """Runs `redline fuzz --help-tunables` and parses the `flags.*` entries."""
  proc = subprocess.run([redline_bin, 'fuzz', '--help-tunables'],
                        capture_output=True,
                        text=True,
                        check=True)
  return parse_redline_tunables(proc.stdout)


def parse_fuzzilli_profile_table(text, include_wasm=False):
  """Returns `(table_probs, omitted_set)` from `engine/d8_flags.cc`.

  Computes the unconditional marginal probability of every flag in
  `kFuzzilliFlagTable` directly from the table rows (`kGroup`, parent flags,
  `kBoolFlag`, coin choices and `kCpuUniform` count), without separate
  probability literals in Python. Without `include_wasm`, skips the Wasm rows
  and puts their flags into `omitted_set`. `omitted_set` also has the flags of
  the "Deferred flags" comment.
  """
  omitted = set()
  comment_match = re.search(r'// Deferred flags[^\n]*\n((?://[^\n]*\n)+)', text)
  if comment_match:
    for flag in re.findall(r'--[a-z0-9-]+', comment_match.group(1)):
      omitted.add(normalize_flag(flag))

  rows = []
  for match in TABLE_ENTRY.finditer(text):
    cond, kind, prob_text, raw_flag, _, _, wasm = match.groups()
    rows.append((cond, kind, float(prob_text), raw_flag, wasm == 'true'))

  cpu_count = sum(1 for row in rows if row[1] == 'kCpuUniform')
  group_probs = {}
  probs = {}
  prev_eff = 0.0

  def p(flag):
    return probs.get(flag, 0.0)

  def cond_prob(cond):
    if cond in ('kTop', 'kIfNotSandbox'):
      return 1.0
    if cond == 'kIfNoTurbofan':
      return p('--no-turbofan')
    if cond == 'kElseNoTurbofan':
      return (1.0 - p('--no-turbofan')) * group_probs.get(cond, 0.0)
    if cond == 'kIfNotMaglevTopTier':
      return 1.0 - p('--maglev-as-top-tier')
    if cond == 'kIfTurbofan':
      return 1.0 - p('--no-turbofan')
    if cond == 'kIfMaglev':
      return 1.0 - p('--no-maglev')
    if cond == 'kIfMaglevNonEager':
      return p('--maglev-non-eager-inlining')
    if cond == 'kIfMaglevTurbofan':
      return (1.0 - p('--no-maglev')) * (1.0 - p('--no-turbofan'))
    if cond in ('kIfTurbolev', 'kIfTurbolevNotSandbox'):
      return p('--turbolev')
    if cond == 'kIfTurbolevFuture':
      return p('--turbolev-future')
    if cond == 'kIfTurbolevNoSmallZero':
      return p('--turbolev') - p('--max-inlined-bytecode-size-small')
    if cond == 'kIfTurbofanNoAssertTypes':
      return (1.0 - p('--no-turbofan')) * (1.0 - p('--assert-types'))
    if cond == 'kIfProtoAssignSeq':
      return p('--proto-assign-seq-lazy-func-opt')
    if cond == 'kIfMaglevOrTurbolev':
      return 1.0 - p('--no-maglev') * (1.0 - p('--turbolev'))
    if cond == 'kIfNoLiftoff':
      return p('--no-liftoff')
    return group_probs.get(cond, 1.0)

  for cond, kind, prob, raw_flag, wasm in rows:
    if wasm and not include_wasm:
      if raw_flag:
        omitted.add(normalize_flag(raw_flag))
      prev_eff = 0.0
      continue
    if kind == 'kGroup':
      group_probs[cond] = prob
      prev_eff = 0.0
      continue
    mult = cond_prob(cond)
    flag = normalize_flag(raw_flag)
    if kind == 'kBoolFlag':
      pos_eff = mult * prob
      neg_eff = mult * (1.0 - prob)
      neg_flag = _flip_polarity(flag)
      probs[flag] = _combine_indep(p(flag), pos_eff)
      probs[neg_flag] = _combine_indep(p(neg_flag), neg_eff)
      prev_eff = 0.0
      continue
    if kind in ('kCoinFirst', 'kCoinSecond', 'kRangeCoinFirst',
                'kRangeCoinSecond'):
      eff = mult * prob * 0.5
      prev_eff = 0.0
    elif kind in ('kRangeWithPrev', 'kRangeCumulative', 'kFlagWithPrev'):
      eff = prev_eff * prob
    elif kind == 'kFlagElse':
      eff = (mult - prev_eff) * prob
      prev_eff = eff
    elif kind == 'kCpuUniform':
      eff = mult * prob / cpu_count if cpu_count > 0 else 0.0
      prev_eff = 0.0
    else:
      eff = mult * prob
      prev_eff = eff
    if cond in EXCLUSIVE_CONDS:
      probs[flag] = p(flag) + eff
    else:
      probs[flag] = _combine_indep(p(flag), eff)

  return {flag: [round(v, 6)] for flag, v in probs.items()}, omitted


def read_table_flag_names(text):
  """Returns the flag names of `kFuzzilliFlagTable` and `RandomD8Flags`."""
  names = set()
  for match in TABLE_ENTRY.finditer(text):
    if match.group(4):
      names.add(normalize_flag(match.group(4)))
  func = re.search(r'\nstd::vector<std::string> RandomD8Flags\(', text)
  if func:
    body_start = text.index('{', func.end())
    body = text[body_start:_find_matching(text, body_start, '{', '}')]
    for flag in re.findall(r'"(--[a-z0-9_-]+)', body):
      names.add(normalize_flag(flag))
  return names


def parse_d8_help_names(help_text):
  """Returns the flag names (`--foo`) that `d8 --help` prints."""
  return {normalize_flag(m.group(1)) for m in D8_HELP_FLAG.finditer(help_text)}


def unknown_d8_flags(names, help_names):
  """Returns the sorted names of `names` that `help_names` does not have."""
  unknown = []
  for name in sorted(names):
    if name in help_names:
      continue
    if name.startswith('--no-') and '--' + name[5:] in help_names:
      continue
    unknown.append(name)
  return unknown


def parse_trials(text):
  """Returns `{flag: probability}` from `clusterfuzz_trials_config.json`."""
  result = {}
  for entry in json.loads(text):
    if entry.get('app_name') != 'd8':
      continue
    prob = float(entry['probability'])
    for token in entry.get('app_args', '').split():
      if token.startswith('--'):
        flag = normalize_flag(token)
        result[flag] = round(result.get(flag, 0.0) + prob, 6)
  return result


def parse_foozzie(text):
  """Returns `{flag: probability}` from `v8_fuzz_flags.json`."""
  result = {}
  for prob, args in json.loads(text):
    for token in args.split():
      if token.startswith('--'):
        flag = normalize_flag(token)
        result[flag] = round(result.get(flag, 0.0) + float(prob), 6)
  return result


def _find_matching(text, start, open_char, close_char):
  """Returns the index after the matching `close_char`."""
  depth = 1
  i = start + 1
  while i < len(text) and depth > 0:
    if text[i] == open_char:
      depth += 1
    elif text[i] == close_char:
      depth -= 1
    i += 1
  return i


def _split_top_level(expr, sep):
  """Splits `expr` at each `sep` that is not inside parentheses."""
  parts = []
  depth = 0
  start = 0
  i = 0
  while i < len(expr):
    if expr[i] == '(':
      depth += 1
    elif expr[i] == ')':
      depth -= 1
    elif depth == 0 and expr.startswith(sep, i):
      parts.append(expr[start:i].strip())
      i += len(sep)
      start = i
      continue
    i += 1
  parts.append(expr[start:].strip())
  return parts


def _strip_parens(term):
  term = term.strip()
  while term.startswith('(') and _find_matching(term, 0, '(', ')') == len(term):
    term = term[1:-1].strip()
  return term


def _eval_probability_arg(arg, config):
  """Evaluates the argument of `probability(...)`, also `c ? a : b`."""
  match = re.fullmatch(
      r'\s*(!?)config\.(\w+)\s*\?\s*([0-9.]+)\s*:\s*([0-9.]+)\s*', arg)
  if match:
    on = bool(config.get(match.group(2))) != bool(match.group(1))
    return float(match.group(3) if on else match.group(4))
  return float(arg)


def _eval_swift_cond(cond, contains_prob, config):
  """Returns `(probability, negated_flags)` of a Swift `if` condition.

  Evaluates `&&`, `||`, `!`, `probability(...)`, `args.contains(...)`,
  `config.*` and `forSandbox` (false). `contains_prob(flag)` gives the
  probability of `args.contains(flag)`, or None to treat the term as true.
  Other names are true. `negated_flags` are the flags of the
  `!args.contains(...)` terms of the top-level `&&` chain.
  """

  def term_prob(term):
    term = _strip_parens(term)
    alternatives = _split_top_level(term, '||')
    if len(alternatives) > 1:
      none = 1.0
      for alternative in alternatives:
        none *= 1.0 - term_prob(alternative)
      return 1.0 - none
    negated = term.startswith('!')
    body = _strip_parens(term[1:]) if negated else term
    prob_match = re.fullmatch(r'probability\((.*)\)', body, re.DOTALL)
    contains_match = SWIFT_CONTAINS.fullmatch(body)
    config_match = re.fullmatch(r'config\.(\w+)', body)
    if prob_match:
      value = _eval_probability_arg(prob_match.group(1), config)
    elif contains_match:
      value = contains_prob(normalize_flag(contains_match.group(1)))
      if value is None:
        return 1.0
    elif config_match:
      value = 1.0 if config.get(config_match.group(1)) else 0.0
    elif body == 'forSandbox':
      value = 0.0
    else:
      return 1.0
    return 1.0 - value if negated else value

  prob = 1.0
  negated_flags = set()
  for term in _split_top_level(cond, '&&'):
    prob *= term_prob(term)
    match = re.fullmatch(r'!\s*' + SWIFT_CONTAINS.pattern, term.strip())
    if match:
      negated_flags.add(normalize_flag(match.group(1)))
  return prob, negated_flags


def parse_fuzzilli(text,
                   bool_defaults=None,
                   redline_flags=None,
                   unconditional=False,
                   config=None):
  """Returns `{flag: probability}` from `V8CommonProfile.swift`.

  When `unconditional` is True (used for `--profile=fuzzilli` and
  `--check-draws`), computes the unconditional marginal probability of each
  flag across all `if` / `else if` branches, `args.contains(...)` guards, and
  `chooseBooleanFlag` / `chooseBooleanFlagRandomly` polarities (`--X` and
  `--no-X`). Inside a block, `args.contains(F)` of a flag F that only that
  block adds uses the probability of F in the block. A guard
  `!args.contains(F)` makes a new draw of F exclude the old draws of F, so
  the probabilities add.

  When `unconditional` is False (used for `--profile=tunables`), uses the
  conditional probability inside preset parent blocks and emits the
  non-default polarity of `chooseBooleanFlag` according to `bool_defaults`.
  """
  redline_flags = set(redline_flags or ())
  config = SWIFT_CONFIG if config is None else config
  func_match = re.search(
      r'public\s+func\s+v8ProcessArgs\([^)]*\)\s*->\s*\[String\]\s*\{', text)
  if not func_match:
    return {}
  body_start = func_match.end() - 1
  body_end = _find_matching(text, body_start, '{', '}')
  body = text[body_start + 1:body_end - 1]
  guard_match = re.search(r'guard\s+(?:config\.)?randomize\s+else\s*\{[^}]*\}',
                          body)
  if guard_match:
    body = body[guard_match.end():]
  body = '\n'.join(re.sub(r'//.*', '', line) for line in body.splitlines())

  result = {}
  # The flags that each enclosing block adds, innermost last.
  frames = [result]

  def add_prob(flag, eff, exclusive):
    if not unconditional:
      result[flag] = round(result.get(flag, 0.0) + eff, 6)
      return
    for frame in frames:
      old = frame.get(flag, 0.0)
      frame[flag] = old + eff if exclusive else _combine_indep(old, eff)

  def walk(block, mult, exclusive):
    frame = frames[-1]

    def contains_prob(flag):
      if not unconditional:
        return None
      if frame is not result and flag in frame:
        return min(1.0, frame[flag] / mult)
      return result.get(flag, 0.0)

    i = 0
    n = len(block)
    parent_conditioned = False
    last_if_prob = 0.0
    while i < n:
      while i < n and block[i].isspace():
        i += 1
      if i >= n:
        break
      helper_match = re.match(r'func\s+chooseBooleanFlag[^{]*\{', block[i:])
      if helper_match:
        brace = i + helper_match.end() - 1
        i = _find_matching(block, brace, '{', '}')
        continue
      if_match = re.match(r'(else\s+)?if\s+([^{]+)\{', block[i:])
      if if_match:
        is_else = bool(if_match.group(1))
        brace = i + if_match.end() - 1
        end = _find_matching(block, brace, '{', '}')
        inner = block[brace + 1:end - 1]
        branch_prob, negated = _eval_swift_cond(
            if_match.group(2), contains_prob, config)
        if unconditional:
          if is_else:
            eff_branch = (1.0 - last_if_prob) * branch_prob
            last_if_prob += eff_branch
          else:
            eff_branch = branch_prob
            last_if_prob = branch_prob
          child_mult = mult * eff_branch
        else:
          child_mult = (
              branch_prob if parent_conditioned else mult * branch_prob)
        if child_mult > 0.0:
          frames.append({})
          walk(inner, child_mult, exclusive | negated)
          frames.pop()
        i = end
        continue
      append_match = re.match(r'args\.append\(', block[i:])
      if append_match:
        paren = i + append_match.end() - 1
        end = _find_matching(block, paren, '(', ')')
        expr = block[paren + 1:end - 1]
        raw_flags = [
            normalize_flag(f) for f in re.findall(r'"(--[a-z0-9_-]+)', expr)
        ]
        if any(
            f in PRESET_PARENT_FLAGS or f in redline_flags for f in raw_flags):
          parent_conditioned = True
        is_choice = ('chooseUniform' in expr or 'Bool.random()' in expr or
                     '?' in expr)
        divisor = len(raw_flags) if (is_choice and raw_flags) else 1
        for flag in raw_flags:
          add_prob(flag, mult / divisor, flag in exclusive)
        i = end
        continue
      bool_match = BOOL_HELPER_CALL.match(block[i:])
      if bool_match:
        name = bool_match.group(1)
        neg = '--no-' + name
        pos = '--' + name
        if unconditional or bool_defaults is None:
          add_prob(pos, mult * 0.5, False)
          add_prob(neg, mult * 0.5, False)
        else:
          flag = neg if bool_defaults.get(name) is True else pos
          add_prob(flag, mult * 0.5, False)
        i += bool_match.end()
        continue
      newline = block.find('\n', i)
      i = newline + 1 if newline != -1 else n

  walk(body, 1.0, frozenset())
  if unconditional:
    return {flag: round(p, 6) for flag, p in result.items()}
  return result


def check_draw_frequencies(redline_bin,
                           swift_text,
                           omitted=None,
                           n=100000,
                           seed=1,
                           max_sigma=DEFAULT_MAX_SIGMA,
                           preset='fuzzilli'):
  """Draws `n` sets from `redline flags` and compares frequencies with Swift."""
  omitted_all = _expand_wasm_polarities(omitted or ())
  implied = parse_fuzzilli(swift_text, unconditional=True)
  proc = subprocess.run([
      redline_bin,
      'flags',
      f'--preset={preset}',
      f'--n={n}',
      f'--seed={seed}',
  ],
                        capture_output=True,
                        text=True,
                        check=True)
  counts = collections.Counter()
  lines = proc.stdout.splitlines()
  total = len(lines)
  if total != n:
    raise RuntimeError(f'expected {n} draws from {redline_bin}, got {total}')
  for line in lines:
    for flag in {normalize_flag(token) for token in line.split()}:
      counts[flag] += 1

  expected = {
      flag: prob for flag, prob in implied.items() if flag not in omitted_all
  }
  diffs = []
  for flag in sorted(set(expected) | set(counts)):
    exp_prob = expected.get(flag, 0.0)
    obs_freq = counts[flag] / total if total > 0 else 0.0
    abs_diff = abs(obs_freq - exp_prob)
    if total > 0 and 0.0 < exp_prob < 1.0:
      sigma = math.sqrt(exp_prob * (1.0 - exp_prob) / total)
      z_sigma = abs_diff / sigma
    else:
      z_sigma = float('inf') if abs_diff > 0.0 else 0.0
    diffs.append({
        'flag': flag,
        'observed': obs_freq,
        'implied': exp_prob,
        'abs_diff': abs_diff,
        'sigma': z_sigma,
        'exceeded': z_sigma > max_sigma,
    })
  diffs.sort(key=lambda item: (-item['sigma'], -item['abs_diff'], item['flag']))
  return diffs


def compare_flags(redline,
                  sources,
                  bool_defaults=None,
                  implications=None,
                  aliases=None,
                  d8_active_flags=None,
                  base_d8_args=None,
                  omitted_wasm=None):
  """Compares `redline` (`{flag: [prob, ...]}`) with `sources`.

  `sources` is `{source_name: {flag: prob}}` in priority order. Returns a dict
  with `missing`, `different_probability`, `redline_only`, and `omitted_wasm`.
  """
  bool_defaults = bool_defaults or {}
  implications = implications or {}
  aliases = aliases or {}
  omitted_wasm = set(omitted_wasm or ())
  omitted_wasm_all = _expand_wasm_polarities(omitted_wasm)
  base_set = {normalize_flag(a) for a in (base_d8_args or ())}
  source_names = list(sources.keys())
  all_source_flags = set()
  for src in sources.values():
    all_source_flags.update(src.keys())
  all_source_flags -= omitted_wasm_all
  redline_set = set(redline.keys())

  missing = []
  for flag in sorted(all_source_flags - redline_set):
    d8_on = None
    if d8_active_flags is not None and flag in d8_active_flags:
      default_state = is_default_on(flag, bool_defaults)
      if flag in base_set or default_state is False:
        d8_on = 'base flags'
      else:
        d8_on = 'default'
    matched_aliases = [
        target for target in aliases.get(flag, ()) if target in redline_set
    ]
    implied_by = []
    for when_flag, implied_token in implications.get(flag, ()):
      if when_flag in redline_set:
        if implied_token != flag:
          implied_by.append(f'{when_flag} -> {implied_token}')
        else:
          implied_by.append(when_flag)
    entry = {
        'flag': flag,
        'sources': {
            name: sources[name][flag]
            for name in source_names
            if flag in sources[name]
        },
    }
    if d8_on is not None:
      entry['d8_on'] = d8_on
    if matched_aliases:
      entry['aliases'] = matched_aliases
    if implied_by:
      entry['implied_by'] = implied_by
    missing.append(entry)

  redline_only = []
  for flag in sorted(redline_set - all_source_flags):
    redline_only.append({
        'flag': flag,
        'redline': redline[flag],
    })

  different = []
  for flag in sorted(redline_set & all_source_flags):
    rl_probs = redline[flag]
    src_probs = {
        name: sources[name][flag]
        for name in source_names
        if flag in sources[name]
    }
    primary_name = next(name for name in source_names if name in src_probs)
    primary_prob = src_probs[primary_name]
    matches_primary = all(
        abs(rp - primary_prob) <= PROB_TOLERANCE for rp in rl_probs)
    any_diff = any(
        abs(rp - sp) > PROB_TOLERANCE
        for rp in rl_probs
        for sp in src_probs.values())
    if any_diff:
      different.append({
          'flag': flag,
          'redline': rl_probs,
          'sources': src_probs,
          'primary': primary_name,
          'matches_primary': matches_primary,
      })

  return {
      'missing': missing,
      'different_probability': different,
      'redline_only': redline_only,
      'omitted_wasm': sorted(omitted_wasm),
  }


def _fmt_prob(prob):
  return f'{prob:g}'


def format_report(report, freq_diffs=None, max_sigma=DEFAULT_MAX_SIGMA):
  """Formats the comparison result as human-readable text."""
  missing = report['missing']
  different = report['different_probability']
  only_rl = report['redline_only']
  omitted_wasm = report.get('omitted_wasm', [])
  primary_diffs = [d for d in different if not d['matches_primary']]
  secondary_diffs = [d for d in different if d['matches_primary']]

  lines = []
  if omitted_wasm:
    lines.append(f'Flags omitted from the profile ({len(omitted_wasm)}): '
                 f'{", ".join(omitted_wasm)}\n')
  lines.append(f'Flags missing in Redline ({len(missing)}):')
  for item in missing:
    srcs = ', '.join(f'{k}={_fmt_prob(v)}' for k, v in item['sources'].items())
    notes = []
    if item.get('d8_on'):
      notes.append(f'already on in d8: {item["d8_on"]}')
    if item.get('aliases'):
      notes.append(f'alias of {", ".join(item["aliases"])}')
    if item.get('implied_by'):
      notes.append(f'implied by {", ".join(item["implied_by"])}')
    suffix = f' [{"; ".join(notes)}]' if notes else ''
    lines.append(f'  {item["flag"]} ({srcs}){suffix}')

  lines.append(f'\nFlags with a different probability ({len(different)}: '
               f'{len(primary_diffs)} differ from primary source, '
               f'{len(secondary_diffs)} match primary source):')
  for item in primary_diffs + secondary_diffs:
    rl_str = ','.join(_fmt_prob(p) for p in item['redline'])
    srcs = ', '.join(f'{k}={_fmt_prob(v)}' for k, v in item['sources'].items())
    tag = (f'matches {item["primary"]}'
           if item['matches_primary'] else 'differs from all sources')
    lines.append(f'  {item["flag"]}: redline={rl_str} ({srcs}) [{tag}]')

  lines.append(f'\nFlags drawn only by Redline ({len(only_rl)}):')
  for item in only_rl:
    rl_str = ','.join(_fmt_prob(p) for p in item['redline'])
    lines.append(f'  {item["flag"]}: redline={rl_str}')

  if freq_diffs:
    exceeded = [item for item in freq_diffs if item.get('exceeded')]
    shown = list(freq_diffs[:5])
    seen_flags = {item['flag'] for item in shown}
    for item in exceeded:
      if item['flag'] not in seen_flags:
        shown.append(item)
        seen_flags.add(item['flag'])
    lines.append(
        f'\nDraw frequency check vs Swift model ({len(exceeded)} flag(s) '
        f'above {max_sigma:g} sigma, top {len(shown)} shown):')
    for item in shown:
      sigma_val = item.get('sigma', 0.0)
      tag = ' [EXCEEDS TOLERANCE]' if item.get('exceeded') else ''
      lines.append(f'  {item["flag"]}: observed={item["observed"]:.5f}, '
                   f'implied={item["implied"]:.5f}, '
                   f'diff={item["abs_diff"]:.5f} ({sigma_val:.2f} sigma){tag}')

  return '\n'.join(lines)


def resolve_fuzzilli_path(path):
  """Returns the path to `V8CommonProfile.swift` from a root or file path."""
  if os.path.isdir(path):
    return os.path.join(path, FUZZILLI_PROFILE_REL)
  return path


def main(argv=None):
  parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
  parser.add_argument('--redline', default=DEFAULT_REDLINE)
  parser.add_argument('--v8-root', default=V8_ROOT)
  parser.add_argument(
      '--profile',
      choices=('tunables', 'fuzzilli', 'fuzzilli-wasm'),
      default='tunables',
      help='compare tunables (default) or the fuzzilli profile table, '
      'without or with its Wasm rows')
  parser.add_argument(
      '--d8',
      help='optional d8 binary to check which missing flags are already on')
  parser.add_argument(
      '--debug-bool',
      action='store_true',
      default=False,
      help='treat DEBUG_BOOL in flag-definitions.h as true (default: false)')
  parser.add_argument('--trials')
  parser.add_argument('--foozzie')
  parser.add_argument(
      '--fuzzilli',
      help='path to the Fuzzilli checkout or V8CommonProfile.swift')
  parser.add_argument(
      '--check-draws',
      type=int,
      default=0,
      help='draw N flag sets for --profile=fuzzilli and compare frequencies')
  parser.add_argument(
      '--max-sigma',
      type=float,
      default=DEFAULT_MAX_SIGMA,
      help='tolerance in standard deviations for --check-draws (default: 4.0)')
  parser.add_argument('--seed', type=int, default=1)
  parser.add_argument(
      '--check-d8',
      help='d8 binary: report flag names of d8_flags.cc that `d8 --help` '
      'does not print')
  parser.add_argument(
      '--strict',
      action='store_true',
      help='exit with status 1 when any difference is found')
  args = parser.parse_args(argv)

  flag_defs_path = os.path.join(args.v8_root, 'src', 'flags',
                                'flag-definitions.h')
  bool_defaults = {}
  implications = {}
  aliases = {}
  if os.path.isfile(flag_defs_path):
    with open(flag_defs_path, encoding='utf-8') as f:
      flag_defs_text = f.read()
    bool_defaults = parse_bool_defaults(
        flag_defs_text, debug_bool=args.debug_bool)
    implications = parse_flag_implications(flag_defs_text)
    aliases = parse_flag_aliases(flag_defs_text)

  if args.check_draws < 0:
    sys.exit('--check-draws must be non-negative')
  fuzzilli_profile = args.profile in ('fuzzilli', 'fuzzilli-wasm')
  if args.check_draws > 0 and not fuzzilli_profile:
    sys.exit('--check-draws requires --profile=fuzzilli or fuzzilli-wasm')

  d8_flags_cc = os.path.join(args.v8_root, 'experimental', 'redline', 'engine',
                             'd8_flags.cc')
  unknown_names = []
  if args.check_d8:
    with open(d8_flags_cc, encoding='utf-8') as f:
      table_names = read_table_flag_names(f.read())
    proc = subprocess.run([args.check_d8, '--help'],
                          capture_output=True,
                          text=True,
                          check=False)
    unknown_names = unknown_d8_flags(table_names,
                                     parse_d8_help_names(proc.stdout))
    print(f'Flag names that {args.check_d8} does not have '
          f'({len(unknown_names)} of {len(table_names)}):')
    for name in unknown_names:
      print(f'  {name}')
    print()

  omitted_wasm = set()
  freq_diffs = None
  if fuzzilli_profile:
    if not args.fuzzilli:
      sys.exit(f'--profile={args.profile} requires --fuzzilli=PATH')
    with open(d8_flags_cc, encoding='utf-8') as f:
      redline, omitted_wasm = parse_fuzzilli_profile_table(
          f.read(), include_wasm=args.profile == 'fuzzilli-wasm')
    fuzzilli_file = resolve_fuzzilli_path(args.fuzzilli)
    with open(fuzzilli_file, encoding='utf-8') as f:
      swift_text = f.read()
    sources = {
        'fuzzilli': parse_fuzzilli(swift_text, unconditional=True),
    }
    if args.check_draws > 0:
      freq_diffs = check_draw_frequencies(
          args.redline,
          swift_text,
          omitted=omitted_wasm,
          n=args.check_draws,
          seed=args.seed,
          max_sigma=args.max_sigma,
          preset='wasm' if args.profile == 'fuzzilli-wasm' else 'fuzzilli')
  else:
    trials_path = args.trials or os.path.join(args.v8_root, 'tools',
                                              'clusterfuzz', 'trials',
                                              'clusterfuzz_trials_config.json')
    foozzie_path = args.foozzie or os.path.join(
        args.v8_root, 'tools', 'clusterfuzz', 'foozzie', 'v8_fuzz_flags.json')
    redline = read_redline_flags(args.redline)
    with open(trials_path, encoding='utf-8') as f:
      trials = parse_trials(f.read())
    with open(foozzie_path, encoding='utf-8') as f:
      foozzie = parse_foozzie(f.read())
    sources = {}
    if args.fuzzilli:
      fuzzilli_file = resolve_fuzzilli_path(args.fuzzilli)
      with open(fuzzilli_file, encoding='utf-8') as f:
        sources['fuzzilli'] = parse_fuzzilli(f.read(), bool_defaults, redline)
    sources['trials'] = trials
    sources['foozzie'] = foozzie

  d8_active_flags = None
  base_d8_args = None
  if args.d8:
    base_d8_args = load_base_d8_args(args.redline)
    d8_active_flags = read_d8_active_flags(args.d8, base_d8_args)

  report = compare_flags(
      redline,
      sources,
      bool_defaults=bool_defaults,
      implications=implications,
      aliases=aliases,
      d8_active_flags=d8_active_flags,
      base_d8_args=base_d8_args,
      omitted_wasm=omitted_wasm)
  print(format_report(report, freq_diffs=freq_diffs, max_sigma=args.max_sigma))
  has_freq_error = bool(
      freq_diffs and any(d.get('exceeded') for d in freq_diffs))
  has_diff = bool(report['missing'] or report['different_probability'] or
                  report['redline_only'])
  has_diff = has_diff or bool(unknown_names)
  return 1 if (has_freq_error or (args.strict and has_diff)) else 0


if __name__ == '__main__':
  sys.exit(main())
