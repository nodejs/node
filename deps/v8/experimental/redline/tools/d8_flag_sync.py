#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compares the random d8 flags of Redline with V8 and Fuzzilli sources.

Reads the flag names and probabilities from `redline fuzz --help-tunables` and
compares them with `tools/clusterfuzz/trials/clusterfuzz_trials_config.json`,
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

Exits with status 0 by default, or 1 when `--strict` is set and a difference
exists.
"""

import argparse
import json
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
PROB_TOLERANCE = 1e-4
PRESET_PARENT_FLAGS = frozenset(
    ('--no-turbofan', '--turbolev', '--turbolev-future'))


def normalize_flag(raw):
  """Returns the canonical `--flag-name` without a `=value` suffix."""
  name = raw.split('=', 1)[0].strip()
  return name.replace('_', '-')


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


def parse_fuzzilli(text, bool_defaults=None, redline_flags=None):
  """Returns `{flag: probability}` from `V8CommonProfile.swift`.

  Approximations of this parser:
    - Scans the body of `v8ProcessArgs` after `guard randomize else` with
      brace matching and regular expressions instead of a Swift AST parser.
    - Treats non-probability conditions such as `!forSandbox` or
      `!args.contains(...)` as true (`probability = 1.0`).
    - Uses the `probability(p)` value of `else if` branches directly without
      subtracting the probability of earlier `if` branches.
    - Uses the conditional probability inside a block when the block sets a
      preset parent flag (`--no-turbofan`, `--turbolev`, `--turbolev-future`)
      or a flag that Redline also draws.
    - Splits the probability equally across all flag literals in an
      `args.append(...)` call that uses `chooseUniform`, `Bool.random()`, or
      `?`.
    - Attributes `0.5 * mult` of `chooseBooleanFlag("name")` to `--no-name`
      when `name` defaults to `true` in `bool_defaults` or `--no-name` is in
      `redline_flags`, and to `--name` otherwise.
  """
  bool_defaults = bool_defaults or {}
  redline_flags = set(redline_flags or ())
  func_match = re.search(
      r'public\s+func\s+v8ProcessArgs\([^)]*\)\s*->\s*\[String\]\s*\{', text)
  if not func_match:
    return {}
  body_start = func_match.end() - 1
  body_end = _find_matching(text, body_start, '{', '}')
  body = text[body_start + 1:body_end - 1]
  guard_match = re.search(r'guard\s+randomize\s+else\s*\{[^}]*\}', body)
  if guard_match:
    body = body[guard_match.end():]
  body = '\n'.join(re.sub(r'//.*', '', line) for line in body.splitlines())

  result = {}

  def walk(block, mult):
    i = 0
    n = len(block)
    parent_conditioned = False
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
      if_match = re.match(r'(?:else\s+)?if\s+([^{]+)\{', block[i:])
      if_block = bool(if_match)
      if if_block:
        cond = if_match.group(1)
        brace = i + if_match.end() - 1
        end = _find_matching(block, brace, '{', '}')
        inner = block[brace + 1:end - 1]
        prob_match = re.search(r'probability\(([0-9.]+)\)', cond)
        prob = float(prob_match.group(1)) if prob_match else 1.0
        child_mult = prob if parent_conditioned else mult * prob
        walk(inner, child_mult)
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
          result[flag] = round(result.get(flag, 0.0) + mult / divisor, 6)
        i = end
        continue
      bool_match = re.match(r'chooseBooleanFlag\("([a-z0-9-]+)"\)', block[i:])
      if bool_match:
        name = bool_match.group(1)
        neg = '--no-' + name
        pos = '--' + name
        flag = neg if (neg in redline_flags or
                       bool_defaults.get(name) is True) else pos
        result[flag] = round(result.get(flag, 0.0) + mult * 0.5, 6)
        i += bool_match.end()
        continue
      newline = block.find('\n', i)
      i = newline + 1 if newline != -1 else n

  walk(body, 1.0)
  return result


def compare_flags(redline,
                  sources,
                  bool_defaults=None,
                  implications=None,
                  aliases=None,
                  d8_active_flags=None,
                  base_d8_args=None):
  """Compares `redline` (`{flag: [prob, ...]}`) with `sources`.

  `sources` is `{source_name: {flag: prob}}` in priority order. Returns a dict
  with `missing`, `different_probability`, and `redline_only`.
  """
  bool_defaults = bool_defaults or {}
  implications = implications or {}
  aliases = aliases or {}
  base_set = {normalize_flag(a) for a in (base_d8_args or ())}
  source_names = list(sources.keys())
  all_source_flags = set()
  for src in sources.values():
    all_source_flags.update(src.keys())
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
  }


def _fmt_prob(prob):
  return f'{prob:g}'


def format_report(report):
  """Formats the comparison result as human-readable text."""
  missing = report['missing']
  different = report['different_probability']
  only_rl = report['redline_only']
  primary_diffs = [d for d in different if not d['matches_primary']]
  secondary_diffs = [d for d in different if d['matches_primary']]

  lines = []
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
      '--strict',
      action='store_true',
      help='exit with status 1 when any difference is found')
  args = parser.parse_args(argv)

  trials_path = args.trials or os.path.join(args.v8_root, 'tools',
                                            'clusterfuzz', 'trials',
                                            'clusterfuzz_trials_config.json')
  foozzie_path = args.foozzie or os.path.join(
      args.v8_root, 'tools', 'clusterfuzz', 'foozzie', 'v8_fuzz_flags.json')
  flag_defs_path = os.path.join(args.v8_root, 'src', 'flags',
                                'flag-definitions.h')

  redline = read_redline_flags(args.redline)
  with open(trials_path, encoding='utf-8') as f:
    trials = parse_trials(f.read())
  with open(foozzie_path, encoding='utf-8') as f:
    foozzie = parse_foozzie(f.read())

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
      base_d8_args=base_d8_args)
  print(format_report(report))
  has_diff = bool(report['missing'] or report['different_probability'] or
                  report['redline_only'])
  return 1 if (args.strict and has_diff) else 0


if __name__ == '__main__':
  sys.exit(main())
