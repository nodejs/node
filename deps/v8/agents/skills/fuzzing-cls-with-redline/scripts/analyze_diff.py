#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Analyzes a V8 CL or branch diff against origin/main and plans fuzzing arms.

Inspects changed folders, line counts, file types, newly added flags, and
target compatibility without requiring the caller to know Redline internals.
Outputs either a human-readable plan or JSON.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass, field
import json
import os
import re
import subprocess
import sys

ARCH_SUBDIRS = ('arm64', 'arm', 'ia32', 'riscv', 'ppc', 's390', 'loong64',
                'mips64')

DEFAULT_DURATION_SEC = 43200  # 12 hours.

# Matches flags defined in src/flags/flag-definitions.h.
RE_DEFINE_BOOL_CALL = re.compile(
    r'DEFINE_BOOL\(\s*([a-z0-9_]+)\s*,\s*(false|true)\b')
RE_DEFINE_EXP_CALL = re.compile(r'DEFINE_EXPERIMENTAL_FEATURE\(\s*([a-z0-9_]+)')
# Matches feature entries in src/flags/feature-flags.h.
RE_FEATURE_ENTRY = re.compile(
    r'(?:IF_([A-Z0-9_]+)_ENABLED\(\s*)?(JS|WASM|INTERNAL)_FEATURE\(\s*'
    r'([a-z0-9_]+)\s*,|'
    r'IF_([A-Z0-9_]+)_ENABLED\(\s*(JS|WASM|INTERNAL)_FEATURE\s*,\s*'
    r'([a-z0-9_]+)\s*,')
# Conditional feature guards that are read-only false in default x64 builds.
UNSUPPORTED_FEATURE_GUARDS = frozenset({'SPARKPLUG_PLUS', 'REVEC'})

# Test directory defaults per area.
AREA_SPECS = {
    'turbolev': {
        'preset': 'turbolev',
        'flag_profile': 'turbolev',
        'randomize_flags': True,
        'flags': [
            '--verify-turboshaft', '--turboshaft-verify-reductions',
            '--maglev-range-verification', '--maglev-assert',
            '--fuzzilli-enable-optimized-code-coverage'
        ],
        'generators': {},
        'imports': [
            'test/mjsunit/turbolev', 'test/mjsunit/maglev',
            'test/mjsunit/compiler'
        ],
        'jit': True,
    },
    'maglev': {
        'preset': 'maglev',
        'flag_profile': 'maglev',
        'randomize_flags': True,
        'flags': [
            '--maglev-assert', '--maglev-assert-types',
            '--maglev-range-verification', '--maglev-future',
            '--maglev-non-eager-inlining',
            '--fuzzilli-enable-optimized-code-coverage'
        ],
        'generators': {},
        'imports': ['test/mjsunit/maglev'],
        'jit': True,
    },
    'compiler': {
        'preset': 'default',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': [
            '--jit-fuzzing', '--verify-turboshaft',
            '--turboshaft-verify-reductions', '--turbo-verify',
            '--turbo-verify-allocation',
            '--fuzzilli-enable-optimized-code-coverage'
        ],
        'generators': {
            'JitOptimize': 8,
            'JitTricky': 4,
            'Deopt': 3,
            'LazyDeopt': 3,
            'OsrLoop': 3,
            'ElementsKind': 4,
            'ShapeChange': 3,
        },
        'imports': ['test/mjsunit/turboshaft', 'test/mjsunit/compiler'],
        'jit': True,
    },
    'wasm': {
        'preset': 'wasm',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': [
            '--wasm-inlining-ignore-call-counts', '--stress-wasm-memory-moving'
        ],
        'generators': {
            'TypedArrayDecl': 3
        },
        'imports': ['test/mjsunit/wasm'],
        'jit': True,
    },
    'builtins': {
        'preset': 'builtins',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--jit-fuzzing', '--future', '--verify-heap'],
        'generators': {},
        'imports': ['test/mjsunit/es6', 'test/mjsunit/harmony'],
        'jit': False,
    },
    'intl': {
        'preset': 'builtins',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--jit-fuzzing', '--future', '--verify-heap'],
        'generators': {
            'IntlOps': 5,
            'StringOps': 4,
        },
        'imports': ['test/intl', 'test/mjsunit/es6'],
        'jit': False,
    },
    'temporal': {
        'preset': 'builtins',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--jit-fuzzing', '--future', '--verify-heap'],
        'generators': {
            'TemporalOps': 5,
            'IntlOps': 3,
        },
        'imports': ['test/mjsunit/harmony', 'test/mjsunit/es6'],
        'jit': False,
    },
    'parser': {
        'preset': 'parser',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': [
            '--stress-lazy', '--verify-bytecode-full',
            '--private-field-bytecodes'
        ],
        'generators': {},
        'imports': [
            'test/mjsunit/parser', 'test/mjsunit/es6', 'test/mjsunit/harmony'
        ],
        'jit': False,
    },
    'interpreter': {
        'preset': 'default',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': [
            '--verify-bytecode-full', '--stress-lazy-source-positions',
            '--jit-fuzzing'
        ],
        'generators': {
            'JitBaseline': 4,
            'JitOptimize': 5,
            'Deopt': 3,
            'Closure': 3,
            'Destructuring': 3,
            'TryCatch': 3,
        },
        'imports': ['test/mjsunit/ignition', 'test/mjsunit/baseline'],
        'jit': True,
    },
    'deoptimizer': {
        'preset': 'maglev',
        'flag_profile': 'maglev',
        'randomize_flags': True,
        'flags': ['--verify-heap', '--fuzzilli-enable-optimized-code-coverage'],
        'generators': {
            'Deopt': 5,
            'LazyDeopt': 5,
            'OsrLoop': 4
        },
        'imports': ['test/mjsunit/compiler', 'test/mjsunit/maglev'],
        'jit': True,
    },
    'objects_ic_runtime': {
        'preset': 'runtime',
        'flag_profile': 'fuzzilli',
        'randomize_flags': False,
        'flags': ['--stress-ic', '--verify-heap', '--private-field-bytecodes'],
        'generators': {
            'MapTransitionFocus': 4,
            'HomomorphicFeedback': 4,
            'ProtoAssignSeq': 4,
            'ElementsKind': 4,
        },
        'imports': ['test/mjsunit/es6', 'test/mjsunit/regress'],
        'jit': False,
    },
    'heap': {
        'preset': 'default',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': [
            '--verify-heap', '--stress-compaction', '--stress-marking=50',
            '--stress-scavenge=50', '--jit-fuzzing'
        ],
        'generators': {
            'Gc': 4,
            'WorkerSpawn': 2,
            'TypedArrayOps': 4,
            'TypedArrayDecl': 3,
            'ShapeChange': 3,
            'ElementsKind': 3,
            'Closure': 3,
        },
        'imports': [
            'test/mjsunit/weakrefs', 'test/mjsunit/es6', 'test/mjsunit/regress'
        ],
        'jit': True,
    },
    'regexp': {
        'preset': 'builtins',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--jit-fuzzing', '--verify-heap'],
        'generators': {
            'RegExpPaths': 6,
            'RegExpDecl': 5,
            'StringOps': 5
        },
        'imports': ['test/mjsunit/regexp', 'test/mjsunit/es6'],
        'jit': False,
    },
    'json': {
        'preset': 'builtins',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--jit-fuzzing', '--verify-heap'],
        'generators': {
            'JsonRoundTrip': 6,
            'ObjectDecl': 4,
            'ArrayDecl': 4,
            'ShapeChange': 3,
        },
        'imports': ['test/mjsunit/es6', 'test/mjsunit/harmony'],
        'jit': False,
    },
    'strings': {
        'preset': 'builtins',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--shared-strings', '--jit-fuzzing', '--verify-heap'],
        'generators': {
            'StringOps': 6,
            'TemplateDecl': 4,
            'RegExpPaths': 3
        },
        'imports': ['test/mjsunit/shared-memory', 'test/mjsunit/es6'],
        'jit': False,
    },
    'sandbox': {
        'preset': 'default',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--sandbox-fuzzing'],
        'generators': {
            'TypedArrayOps': 4,
            'WasmModule': 3,
            'JitOptimize': 4
        },
        'imports': ['test/mjsunit/sandbox'],
        'jit': True,
    },
    'snapshot': {
        'preset': 'default',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--verify-heap', '--jit-fuzzing'],
        'generators': {
            'SerializerRoundTrip': 6,
            'ShapeChange': 3
        },
        'imports': [
            'test/mjsunit/value-serializer.js', 'test/mjsunit/serialize-ic.js',
            'test/mjsunit/d8'
        ],
        'jit': False,
    },
    'api': {
        'preset': 'builtins',
        'flag_profile': 'fuzzilli',
        'randomize_flags': True,
        'flags': ['--jit-fuzzing', '--expose-fast-api', '--verify-heap'],
        'generators': {
            'HostObjectDecl': 5,
            'FastApiCall': 5
        },
        'imports': ['test/mjsunit/d8', 'test/mjsunit/compiler'],
        'jit': False,
    },
}


@dataclass
class ArmSpec:
  name: str
  build_variant: str
  coverage_mode: str
  builtins_coverage: bool
  change_based: bool
  preset: str
  d8_extra_args: list[str]
  generator_weights: dict[str, float]
  randomize_d8_flags: bool
  d8_flag_profile: str
  threading: str
  timeout_ms: int
  jobs: int
  seeds: str
  rationale: str
  duration_sec: int = DEFAULT_DURATION_SEC
  extra_gn_args: list[str] = field(default_factory=list)


@dataclass
class DiffAnalysis:
  base_commit: str
  head_ref: str
  changed_files: list[str]
  changed_tests: list[str]
  areas: list[str]
  new_flags: list[str]
  has_x64_d8_cpp: bool
  has_arm64_cpp: bool
  has_csa_or_torque: bool
  has_only_non_runtime: bool
  unsupported_arches: list[str]
  supports_bundle_reprl: bool
  supports_v8_redline: bool
  v8_questions: list[str]
  arms: list[ArmSpec] = field(default_factory=list)


def _git(repo: str, *args: str) -> str:
  proc = subprocess.run(
      ['git', '-C', repo, *args],
      capture_output=True,
      text=True,
      check=False,
  )
  if proc.returncode != 0:
    raise RuntimeError(
        f'git {" ".join(args)} failed in {repo}: {proc.stderr.strip()}')
  return proc.stdout


def _path_has_arch(path: str) -> str | None:
  parts = path.split('/')
  for arch in ARCH_SUBDIRS:
    if arch in parts:
      return arch
  return None


TEMPORAL_PATH_PREFIXES = (
    'src/builtins/builtins-temporal',
    'src/objects/js-temporal-',
    'src/runtime/runtime-temporal',
)
INTL_PATH_PREFIXES = (
    'src/builtins/builtins-intl',
    'src/objects/intl-',
    'src/objects/js-break-iterator',
    'src/objects/js-collator',
    'src/objects/js-date-time-format',
    'src/objects/js-display-names',
    'src/objects/js-duration-format',
    'src/objects/js-list-format',
    'src/objects/js-locale',
    'src/objects/js-number-format',
    'src/objects/js-plural-rules',
    'src/objects/js-relative-time-format',
    'src/objects/js-segment',
    'src/runtime/runtime-intl',
)


def _area_for_path(path: str) -> str | None:
  is_src = path.startswith(('src/', 'include/'))
  if path.startswith('src/compiler/turboshaft/maglev') or ('turbolev' in path
                                                           and is_src):
    return 'turbolev'
  if path.startswith('src/maglev/'):
    return 'maglev'
  if path.startswith('src/compiler/wasm') or (
      path.startswith('src/compiler/turboshaft/') and 'wasm' in path):
    return 'wasm'
  if path.startswith(('src/compiler/', 'src/codegen/')):
    return 'compiler'
  if path.startswith('src/wasm/'):
    return 'wasm'
  if path.startswith(TEMPORAL_PATH_PREFIXES):
    return 'temporal'
  if path.startswith(INTL_PATH_PREFIXES):
    return 'intl'
  if path.startswith(('src/builtins/', 'src/torque/', 'src/bigint/')):
    return 'builtins'
  if path.startswith(('src/parsing/', 'src/ast/')):
    return 'parser'
  if path.startswith(('src/interpreter/', 'src/baseline/')):
    return 'interpreter'
  if path.startswith(('src/deoptimizer/', 'src/execution/')):
    return 'deoptimizer'
  if path.startswith(('src/objects/', 'src/ic/', 'src/runtime/')):
    return 'objects_ic_runtime'
  if path.startswith('src/heap/'):
    return 'heap'
  if path.startswith('src/regexp/'):
    return 'regexp'
  if path.startswith('src/json/'):
    return 'json'
  if path.startswith('src/strings/'):
    return 'strings'
  if path.startswith('src/sandbox/'):
    return 'sandbox'
  if path.startswith('src/snapshot/'):
    return 'snapshot'
  if path.startswith(('src/api/', 'src/d8/')):
    return 'api'
  return None


def _parse_numstat(numstat_text: str) -> dict[str, int]:
  counts: dict[str, int] = {}
  for line in numstat_text.splitlines():
    parts = line.split('\t')
    if len(parts) < 3:
      continue
    add_s, del_s, path = parts[0], parts[1], parts[-1].strip()
    if ' => ' in path:
      path = re.sub(r'\{[^{}]* => ([^{}]*)\}', r'\1', path)
      if ' => ' in path:
        path = path.split(' => ', 1)[1]
    added = int(add_s) if add_s.isdigit() else 1
    deleted = int(del_s) if del_s.isdigit() else 1
    counts[path] = max(1, added + deleted)
  return counts


def classify_files(
    files: list[str],
    line_counts: dict[str, int] | None = None,
) -> dict[str, object]:
  """Classifies changed V8 paths into ranked areas and build eligibility."""
  area_weights: dict[str, int] = {}
  area_order: list[str] = []
  changed_tests: list[str] = []
  unsupported_arches: list[str] = []

  has_x64_d8_cpp = False
  has_arm64_cpp = False
  has_csa_or_torque = False
  has_any_runtime = False

  for path in files:
    is_cpp = path.endswith(('.cc', '.h', '.inl'))
    is_src = path.startswith(('src/', 'include/'))
    arch = _path_has_arch(path)
    weight = (line_counts or {}).get(path, 1)

    if path.startswith('test/mjsunit/') and path.endswith('.js'):
      if os.path.basename(path) != 'mjsunit.js' and path not in changed_tests:
        changed_tests.append(path)

    if path.endswith('.tq') or path.startswith('src/torque/') or (
        path.startswith('src/builtins/') and path.endswith('-gen.cc')):
      has_csa_or_torque = True
      has_any_runtime = True
    elif is_src and is_cpp:
      has_any_runtime = True
      if arch == 'arm64':
        has_arm64_cpp = True
      elif arch is None or arch == 'x64':
        has_x64_d8_cpp = True
      elif arch not in unsupported_arches:
        unsupported_arches.append(arch)

    area = _area_for_path(path)
    if area:
      if area not in area_weights:
        area_order.append(area)
        area_weights[area] = 0
      area_weights[area] += weight

  # Rank areas by changed lines descending so large subsystem changes dominate.
  areas = sorted(
      area_order, key=lambda a: (-area_weights[a], area_order.index(a)))
  if not areas and has_any_runtime:
    areas.append('compiler')

  return {
      'areas': areas,
      'changed_tests': changed_tests,
      'has_x64_d8_cpp': has_x64_d8_cpp,
      'has_arm64_cpp': has_arm64_cpp,
      'has_csa_or_torque': has_csa_or_torque,
      'has_only_non_runtime': not has_any_runtime,
      'unsupported_arches': unsupported_arches,
  }


def _collect_added_blocks(diff_text: str, target_file: str) -> list[str]:
  """Joins contiguous added diff lines for target_file into parseable blocks."""
  blocks: list[str] = []
  current_file = ''
  cur: list[str] = []

  def flush() -> None:
    if cur:
      blocks.append(' '.join(cur))
      cur.clear()

  for line in diff_text.splitlines():
    if line.startswith('+++ '):
      flush()
      path = line[4:]
      current_file = path[2:] if path.startswith('b/') else ''
      continue
    if current_file != target_file:
      continue
    if line.startswith('+') and not line.startswith('+++'):
      cur.append(line[1:].strip())
    else:
      flush()
  flush()
  return blocks


def _parse_feature_sections(file_text: str) -> dict[str, str]:
  """Maps each feature flag name to its FOREACH_* macro section."""
  sections: dict[str, str] = {}
  current_section = ''
  buf = ''
  for line in file_text.splitlines():
    m_sec = re.match(r'^#define\s+(FOREACH_[A-Z_]+_FEATURE_FLAG)\b', line)
    if m_sec:
      current_section = m_sec.group(1)
      buf = ''
      continue
    if not current_section:
      continue
    stripped = line.rstrip()
    if not stripped:
      buf = ''
      continue
    buf += ' ' + stripped.rstrip('\\').strip()
    if ')' in buf:
      for m in RE_FEATURE_ENTRY.finditer(buf):
        guard = m.group(1) or m.group(4) or ''
        kind = m.group(2) or m.group(5)
        raw_name = m.group(3) or m.group(6)
        name = raw_name.replace('_', '-')
        flag = '--wasm-' + name if kind == 'WASM' else '--' + name
        if guard in UNSUPPORTED_FEATURE_GUARDS:
          sections[flag] = 'UNSUPPORTED'
        else:
          sections[flag] = current_section
      buf = ''
  return sections


def _parse_debug_only_flags(file_text: str) -> set[str]:
  """Returns flag names defined inside #ifdef DEBUG blocks."""
  debug_flags: set[str] = set()
  stack: list[bool] = []
  in_debug = False
  buf = ''
  for line in file_text.splitlines():
    stripped = line.strip()
    if stripped.startswith('#ifdef DEBUG') or stripped.startswith(
        '#if defined(DEBUG)'):
      stack.append(True)
      in_debug = any(stack)
      continue
    if stripped.startswith('#if'):
      stack.append(False)
      in_debug = any(stack)
      continue
    if stripped.startswith('#else'):
      if stack:
        stack[-1] = not stack[-1] if stack[-1] else False
        in_debug = any(stack)
      continue
    if stripped.startswith('#endif'):
      if stack:
        stack.pop()
      in_debug = any(stack)
      continue
    if not in_debug:
      continue
    buf += ' ' + stripped
    if ')' in buf:
      for m in RE_DEFINE_BOOL_CALL.finditer(buf):
        debug_flags.add('--' + m.group(1).replace('_', '-'))
      for m in RE_DEFINE_EXP_CALL.finditer(buf):
        debug_flags.add('--' + m.group(1).replace('_', '-'))
      buf = ''
  return debug_flags


def extract_new_flags(
    diff_text: str,
    feature_flags_text: str = '',
    flag_definitions_text: str = '',
) -> list[str]:
  """Extracts new default-false CLI flags added in flag and feature headers."""
  flags: list[str] = []
  debug_only = (
      _parse_debug_only_flags(flag_definitions_text)
      if flag_definitions_text else set())
  feature_sections = (
      _parse_feature_sections(feature_flags_text) if feature_flags_text else {})

  for block in _collect_added_blocks(diff_text, 'src/flags/flag-definitions.h'):
    for m in RE_DEFINE_BOOL_CALL.finditer(block):
      if m.group(2) != 'false':
        continue
      flag = '--' + m.group(1).replace('_', '-')
      if flag not in debug_only and flag not in flags:
        flags.append(flag)
    for m in RE_DEFINE_EXP_CALL.finditer(block):
      flag = '--' + m.group(1).replace('_', '-')
      if flag not in debug_only and flag not in flags:
        flags.append(flag)

  default_off_sections = {
      'FOREACH_EXPERIMENTAL_FEATURE_FLAG',
      'FOREACH_PRE_STAGED_FEATURE_FLAG',
  }
  for block in _collect_added_blocks(diff_text, 'src/flags/feature-flags.h'):
    for m in RE_FEATURE_ENTRY.finditer(block):
      guard = m.group(1) or m.group(4) or ''
      if guard in UNSUPPORTED_FEATURE_GUARDS:
        continue
      kind = m.group(2) or m.group(5)
      raw_name = m.group(3) or m.group(6)
      name = raw_name.replace('_', '-')
      flag = '--wasm-' + name if kind == 'WASM' else '--' + name
      if feature_sections:
        sec = feature_sections.get(flag)
        if sec not in default_off_sections:
          continue
      if flag not in flags:
        flags.append(flag)
  return flags


def _merge_unique(base: list[str], extra: list[str]) -> list[str]:
  out = list(base)
  for item in extra:
    if item not in out:
      out.append(item)
  return out


def _allocate_jobs(
    arms: list[ArmSpec],
    vcpus: int,
    ram_gb: int = 0,
) -> list[int]:
  """Allocates worker counts while accounting for thread pools and ASan RAM."""
  if not arms:
    return []
  thread_costs: list[float] = []
  weights: list[float] = []
  for arm in arms:
    # Concurrent arms spawn 3 background threads per worker.
    has_thread_pool = ('--thread-pool-size=' in arm.threading or any(
        f.startswith('--thread-pool-size=') for f in arm.d8_extra_args))
    t_cost = 2.0 if has_thread_pool else 1.0
    thread_costs.append(t_cost)
    if arm.change_based:
      weights.append(1.4)
    elif arm.build_variant == 'asan_slow_dcheck':
      weights.append(0.9)
    elif arm.build_variant == 'arm64sim':
      weights.append(0.8)
    else:
      weights.append(1.0)

  budget = max(float(len(arms)), vcpus * 0.85)
  denom = sum(w * c for w, c in zip(weights, thread_costs)) or 1.0
  jobs = [max(1, int(round(budget * w / denom))) for w in weights]

  # Cap ASan workers by available RAM (roughly 1.5 GB per ASan d8 process).
  if ram_gb > 0:
    asan_cap = max(1, int(ram_gb // 3))
    for idx, arm in enumerate(arms):
      if arm.build_variant == 'asan_slow_dcheck' and jobs[idx] > asan_cap:
        jobs[idx] = asan_cap

  while sum(j * c for j, c in zip(jobs, thread_costs)) > budget + 0.5:
    reducible = [i for i, j in enumerate(jobs) if j > 1]
    if not reducible:
      break
    idx = max(reducible, key=lambda i: jobs[i] * thread_costs[i])
    jobs[idx] -= 1
  return jobs


def _build_questions(
    areas: list[str],
    changed_tests: list[str],
    new_flags: list[str],
    has_x64_d8_cpp: bool,
    has_arm64_cpp: bool,
    has_csa_or_torque: bool,
    unsupported_arches: list[str],
    supports_bundle_reprl: bool,
    supports_v8_redline: bool,
    duration_sec: int,
) -> list[str]:
  questions: list[str] = []
  if new_flags:
    questions.append(
        'Your change adds ' + ', '.join(new_flags) +
        ' (default off). Enable on all builds (Recommended), or test both '
        'on and off?')
  else:
    questions.append(
        'Does this change require any off-by-default V8 flag (for example, '
        '`--future`, `--turbolev`, `--maglev-future`, or `--minor-ms`)?')

  if has_arm64_cpp:
    questions.append(
        'Your change modifies `arm64/` code. Keep the `arm64` simulator build '
        '(`v8_target_cpu="arm64"`) alongside x64 (Recommended)?')
  elif unsupported_arches and not has_x64_d8_cpp and not has_csa_or_torque:
    questions.append(
        'Your change only modifies ' + ', '.join(unsupported_arches) +
        ' architecture files that do not compile into x64 `d8`. Confirm '
        'whether to stop or fuzz a general x64 V8 area instead.')
  elif not changed_tests and any(
      a in ('maglev', 'turbolev', 'compiler', 'wasm') for a in areas):
    questions.append(
        'Are there specific `test/mjsunit/` files or JS/Wasm patterns you '
        'want prioritized, or use the subsystem defaults (Recommended)?')

  if not supports_bundle_reprl or not supports_v8_redline:
    questions.append(
        'This branch is based on an older V8 commit. Can we rebase a separate '
        'copy onto `origin/main` so that crash reproducers work cleanly?')

  if duration_sec <= 0:
    questions.append(
        'No duration cap is set (run until stopped). Keep running until '
        'stopped, or choose a duration (for example, 4 h, 12 h, or 24 h)?')
  else:
    hours = max(1, int(round(duration_sec / 3600.0)))
    questions.append(
        f'Default run duration is {hours} h. Keep {hours} h (Recommended), or '
        'choose another duration (for example, 4 h, 24 h, or until stopped)?')
  return questions


def plan_arms(
    classification: dict[str, object],
    new_flags: list[str],
    vcpus: int,
    ram_gb: int = 0,
    duration_sec: int = DEFAULT_DURATION_SEC,
) -> list[ArmSpec]:
  """Constructs a balanced arm plan from diff classification."""
  areas: list[str] = classification['areas']  # type: ignore[assignment]
  changed_tests: list[str] = list(classification.get(
      'changed_tests', []))  # type: ignore[arg-type]
  has_x64_d8_cpp = bool(classification['has_x64_d8_cpp'])
  has_arm64_cpp = bool(classification['has_arm64_cpp'])
  has_csa_or_torque = bool(classification['has_csa_or_torque'])

  if not areas:
    return []

  primary_area = areas[0]
  spec = AREA_SPECS[primary_area]
  primary_preset = str(spec['preset'])
  primary_profile = str(spec['flag_profile'])
  primary_randomize = bool(spec['randomize_flags'])
  primary_flags = _merge_unique(list(spec['flags']), new_flags)
  primary_gens = dict(spec['generators'])

  imports: list[str] = list(changed_tests[:3])
  for area in areas:
    for p in AREA_SPECS[area]['imports']:
      if p not in imports:
        imports.append(p)
  seed_spec = ','.join(imports[:4])

  # Pure compiler and Wasm C++ changes skip builtins coverage on the counters
  # arm while keeping builtins coverage on ASan and verification arms.
  pure_jit_or_wasm = all(
      a in ('maglev', 'turbolev', 'compiler', 'wasm') for a in areas)
  opt_cov = bool(spec['jit'])
  extra_gn = (['v8_enable_memory_corruption_api=true']
              if 'sandbox' in areas else [])

  arms: list[ArmSpec] = []

  # 1. Required ASan + slow_dchecks arm with full-V8 inline-bool-flag coverage.
  asan_flags = list(primary_flags)
  if opt_cov and '--fuzzilli-enable-optimized-code-coverage' in asan_flags:
    # Keep ASan fast. The counters arm tracks optimized code features.
    asan_flags = [
        f for f in asan_flags
        if f != '--fuzzilli-enable-optimized-code-coverage'
    ]
  arms.append(
      ArmSpec(
          name='arm-asan-slow',
          build_variant='asan_slow_dcheck',
          coverage_mode='inline-bool-flag',
          builtins_coverage=True,
          change_based=False,
          preset=primary_preset,
          d8_extra_args=_merge_unique(asan_flags, ['--single-threaded']),
          generator_weights=primary_gens,
          randomize_d8_flags=primary_randomize,
          d8_flag_profile=primary_profile,
          threading='--single-threaded',
          timeout_ms=800,
          jobs=1,
          seeds=seed_spec,
          rationale=(
              'Memory safety (ASan) and SLOW_DCHECK across all of V8. Catches '
              'UAF, OOB, and downstream regressions outside the diff.'),
          duration_sec=duration_sec,
          extra_gn_args=list(extra_gn),
      ))

  # 2. Change-based counters arm when x64 C++ lines in src/ or include/ changed.
  if has_x64_d8_cpp:
    cb_flags = _merge_unique(primary_flags, ['--thread-pool-size=3'])
    arms.append(
        ArmSpec(
            name='arm-change-counters',
            build_variant='counters_slow_dcheck',
            coverage_mode='inline-8bit-counters,pc-table',
            builtins_coverage=not pure_jit_or_wasm,
            change_based=True,
            preset=primary_preset,
            d8_extra_args=cb_flags,
            generator_weights=primary_gens,
            randomize_d8_flags=primary_randomize,
            d8_flag_profile=primary_profile,
            threading='--thread-pool-size=3',
            timeout_ms=600,
            jobs=1,
            seeds=seed_spec,
            rationale=(
                'CL-targeted counters build with SLOW_DCHECK and concurrent '
                'threads focused on changed lines.'),
            duration_sec=duration_sec,
            extra_gn_args=list(extra_gn),
        ))

  # 3. Secondary tier, verification, or architecture arm.
  secondary_area = areas[1] if len(areas) > 1 else ''
  if has_arm64_cpp:
    arms.append(
        ArmSpec(
            name='arm-arm64sim',
            build_variant='arm64sim',
            coverage_mode=('inline-8bit-counters,pc-table'
                           if not has_x64_d8_cpp else 'inline-bool-flag'),
            builtins_coverage=True,
            change_based=not has_x64_d8_cpp,
            preset=primary_preset,
            d8_extra_args=_merge_unique(primary_flags, ['--single-threaded']),
            generator_weights=primary_gens,
            randomize_d8_flags=primary_randomize,
            d8_flag_profile=primary_profile,
            threading='--single-threaded',
            timeout_ms=1500,
            jobs=1,
            seeds=seed_spec,
            rationale='Exercises arm64 backend paths under the simulator.',
            duration_sec=duration_sec,
            extra_gn_args=list(extra_gn),
        ))
  elif primary_area == 'maglev':
    t_spec = AREA_SPECS['turbolev']
    arms.append(
        ArmSpec(
            name='arm-turbolev-verif',
            build_variant='slow_dcheck',
            coverage_mode='inline-bool-flag',
            builtins_coverage=True,
            change_based=False,
            preset='turbolev',
            d8_extra_args=_merge_unique(
                list(t_spec['flags']) + new_flags, ['--single-threaded']),
            generator_weights={},
            randomize_d8_flags=True,
            d8_flag_profile='turbolev',
            threading='--single-threaded',
            timeout_ms=700,
            jobs=1,
            seeds=seed_spec,
            rationale=(
                'Covers Maglev IR shared with Turbolev and Turboshaft checks.'),
            duration_sec=duration_sec,
            extra_gn_args=list(extra_gn),
        ))
  elif primary_area == 'turbolev' or (primary_area == 'compiler' and
                                      secondary_area == 'maglev'):
    m_spec = AREA_SPECS['maglev']
    arms.append(
        ArmSpec(
            name='arm-maglev-alt',
            build_variant='slow_dcheck',
            coverage_mode='inline-bool-flag',
            builtins_coverage=True,
            change_based=False,
            preset='maglev',
            d8_extra_args=_merge_unique(
                list(m_spec['flags']) + new_flags, ['--single-threaded']),
            generator_weights={},
            randomize_d8_flags=True,
            d8_flag_profile='maglev',
            threading='--single-threaded',
            timeout_ms=700,
            jobs=1,
            seeds=seed_spec,
            rationale='Covers standalone Maglev with Maglev assertions.',
            duration_sec=duration_sec,
            extra_gn_args=list(extra_gn),
        ))
  elif has_csa_or_torque or primary_area in ('builtins', 'intl', 'temporal'):
    t_spec = AREA_SPECS['turbolev']
    arms.append(
        ArmSpec(
            name='arm-builtins-jit',
            build_variant='verif',
            coverage_mode='inline-bool-flag',
            builtins_coverage=True,
            change_based=False,
            preset='turbolev',
            d8_extra_args=_merge_unique(
                list(t_spec['flags']) + new_flags, ['--thread-pool-size=3']),
            generator_weights={
                **primary_gens,
                'DiscoveredApi': 8,
                'BuiltinCall': 6,
            },
            randomize_d8_flags=True,
            d8_flag_profile='turbolev',
            threading='--thread-pool-size=3',
            timeout_ms=700,
            jobs=1,
            seeds=seed_spec,
            rationale=(
                'Covers builtins profiling, verification features, and JIT '
                'builtin calls.'),
            duration_sec=duration_sec,
            extra_gn_args=list(extra_gn),
        ))
  elif primary_area == 'wasm':
    arms.append(
        ArmSpec(
            name='arm-wasm-sync-verif',
            build_variant='verif',
            coverage_mode='inline-bool-flag',
            builtins_coverage=True,
            change_based=False,
            preset='wasm',
            d8_extra_args=_merge_unique(
                primary_flags, ['--wasm-sync-tier-up', '--single-threaded']),
            generator_weights=primary_gens,
            randomize_d8_flags=True,
            d8_flag_profile='fuzzilli',
            threading='--single-threaded',
            timeout_ms=700,
            jobs=1,
            seeds=seed_spec,
            rationale=(
                'Verification build with synchronous Wasm tier-up alongside '
                'concurrent CL-targeted fuzzing.'),
            duration_sec=duration_sec,
            extra_gn_args=list(extra_gn),
        ))
  else:
    if 'sandbox' in areas:
      # Use fuzzilli flag profile on sandbox arms so --verify-heap is never
      # injected randomly alongside --sandbox-fuzzing.
      alt_preset = 'default'
      alt_profile = 'fuzzilli'
      alt_extra = ['--thread-pool-size=3']
    else:
      alt_preset = 'turbolev' if primary_preset != 'turbolev' else 'default'
      alt_profile = 'turbolev' if alt_preset == 'turbolev' else 'fuzzilli'
      alt_extra = ['--thread-pool-size=3', '--verify-heap']
    arms.append(
        ArmSpec(
            name='arm-verif-concurrent',
            build_variant='verif',
            coverage_mode='inline-bool-flag',
            builtins_coverage=True,
            change_based=False,
            preset=alt_preset,
            d8_extra_args=_merge_unique(primary_flags, alt_extra),
            generator_weights=primary_gens,
            randomize_d8_flags=True,
            d8_flag_profile=alt_profile,
            threading='--thread-pool-size=3',
            timeout_ms=700,
            jobs=1,
            seeds=seed_spec,
            rationale=(
                'Verification build under concurrent compilation and GC.'),
            duration_sec=duration_sec,
            extra_gn_args=list(extra_gn),
        ))

  # 4. Optional pure-generative arm on larger Cloudtops (>= 40 vCPUs).
  if vcpus >= 40:
    arms.append(
        ArmSpec(
            name='arm-generative',
            build_variant='slow_dcheck',
            coverage_mode='inline-bool-flag',
            builtins_coverage=True,
            change_based=False,
            preset=primary_preset,
            d8_extra_args=_merge_unique(
                [
                    f for f in primary_flags
                    if f != '--fuzzilli-enable-optimized-code-coverage'
                ],
                ['--single-threaded'],
            ),
            generator_weights=primary_gens,
            randomize_d8_flags=primary_randomize,
            d8_flag_profile=primary_profile,
            threading='--single-threaded',
            timeout_ms=700,
            jobs=1,
            seeds='generative',
            rationale='Pure generative JS without test/mjsunit seed bias.',
            duration_sec=duration_sec,
            extra_gn_args=list(extra_gn),
        ))

  jobs = _allocate_jobs(arms, vcpus, ram_gb=ram_gb)
  for arm, j in zip(arms, jobs):
    arm.jobs = j
  return arms


def resize_plan_file(plan_path: str, vcpus: int, ram_gb: int = 0) -> dict:
  """Rescales arm worker counts in an existing plan.json in place."""
  with open(plan_path, encoding='utf-8') as f:
    plan = json.load(f)
  raw_arms = plan.get('arms', [])
  if not raw_arms:
    raise RuntimeError(f'No arms found in {plan_path}')
  specs = [
      ArmSpec(
          name=str(a.get('name', '')),
          build_variant=str(a.get('build_variant', '')),
          coverage_mode=str(a.get('coverage_mode', '')),
          builtins_coverage=bool(a.get('builtins_coverage', True)),
          change_based=bool(a.get('change_based', False)),
          preset=str(a.get('preset', 'default')),
          d8_extra_args=list(a.get('d8_extra_args', [])),
          generator_weights=dict(a.get('generator_weights', {})),
          randomize_d8_flags=bool(a.get('randomize_d8_flags', True)),
          d8_flag_profile=str(a.get('d8_flag_profile', 'fuzzilli')),
          threading=str(a.get('threading', '')),
          timeout_ms=int(a.get('timeout_ms', 600)),
          jobs=int(a.get('jobs', 1)),
          seeds=str(a.get('seeds', '')),
          rationale=str(a.get('rationale', '')),
          duration_sec=int(a.get('duration_sec', DEFAULT_DURATION_SEC)),
          extra_gn_args=list(a.get('extra_gn_args', [])),
      ) for a in raw_arms
  ]
  jobs = _allocate_jobs(specs, vcpus, ram_gb=ram_gb)
  for raw, j in zip(raw_arms, jobs):
    raw['jobs'] = j
  with open(plan_path, 'w', encoding='utf-8') as f:
    json.dump(plan, f, indent=2)
  return plan


def analyze(
    repo: str,
    base: str,
    head: str,
    include_worktree: bool,
    vcpus: int,
    ram_gb: int = 0,
    duration_sec: int = DEFAULT_DURATION_SEC,
) -> DiffAnalysis:
  merge_base = _git(repo, 'merge-base', base, head).strip()
  diff_target = [] if include_worktree else [head]
  files_out = _git(repo, 'diff', '--name-only', merge_base, *diff_target)
  changed_files = [
      line.strip() for line in files_out.splitlines() if line.strip()
  ]
  numstat_out = _git(repo, 'diff', '--numstat', merge_base, *diff_target)
  line_counts = _parse_numstat(numstat_out)

  if include_worktree:
    untracked_out = _git(repo, 'ls-files', '--others', '--exclude-standard')
    for path in untracked_out.splitlines():
      path = path.strip()
      if path and path not in changed_files:
        changed_files.append(path)
        full = os.path.join(repo, path)
        try:
          with open(full, encoding='utf-8', errors='replace') as f:
            line_counts[path] = max(1, sum(1 for _ in f))
        except OSError:
          line_counts[path] = 1

  diff_text = _git(repo, 'diff', '--unified=0', merge_base, *diff_target)

  def _read_head_file(rel_path: str) -> str:
    if include_worktree:
      full = os.path.join(repo, rel_path)
      try:
        with open(full, encoding='utf-8', errors='replace') as f:
          return f.read()
      except OSError:
        return ''
    proc = subprocess.run(
        ['git', '-C', repo, 'show', f'{head}:{rel_path}'],
        capture_output=True,
        text=True,
        check=False,
    )
    return proc.stdout if proc.returncode == 0 else ''

  classification = classify_files(changed_files, line_counts=line_counts)
  feature_text = (
      _read_head_file('src/flags/feature-flags.h')
      if 'src/flags/feature-flags.h' in changed_files else '')
  flag_defs_text = (
      _read_head_file('src/flags/flag-definitions.h')
      if 'src/flags/flag-definitions.h' in changed_files else '')
  new_flags = extract_new_flags(
      diff_text,
      feature_flags_text=feature_text,
      flag_definitions_text=flag_defs_text,
  )

  supports_bundle_reprl = '--bundle-reprl' in _read_head_file('src/d8/d8.cc')
  supports_v8_redline = 'v8_redline' in _read_head_file('gni/v8.gni')

  areas: list[str] = classification['areas']  # type: ignore[assignment]
  changed_tests: list[str] = list(
      classification['changed_tests'])  # type: ignore[arg-type]
  has_x64_d8_cpp = bool(classification['has_x64_d8_cpp'])
  has_arm64_cpp = bool(classification['has_arm64_cpp'])
  has_csa_or_torque = bool(classification['has_csa_or_torque'])
  unsupported_arches: list[str] = list(
      classification['unsupported_arches'])  # type: ignore[arg-type]

  questions = _build_questions(
      areas,
      changed_tests,
      new_flags,
      has_x64_d8_cpp,
      has_arm64_cpp,
      has_csa_or_torque,
      unsupported_arches,
      supports_bundle_reprl,
      supports_v8_redline,
      duration_sec,
  )
  arms = plan_arms(
      classification,
      new_flags,
      vcpus,
      ram_gb=ram_gb,
      duration_sec=duration_sec,
  )

  return DiffAnalysis(
      base_commit=merge_base,
      head_ref=head,
      changed_files=changed_files,
      changed_tests=changed_tests,
      areas=areas,
      new_flags=new_flags,
      has_x64_d8_cpp=has_x64_d8_cpp,
      has_arm64_cpp=has_arm64_cpp,
      has_csa_or_torque=has_csa_or_torque,
      has_only_non_runtime=bool(classification['has_only_non_runtime']),
      unsupported_arches=unsupported_arches,
      supports_bundle_reprl=supports_bundle_reprl,
      supports_v8_redline=supports_v8_redline,
      v8_questions=questions,
      arms=arms,
  )


def format_markdown(result: DiffAnalysis) -> str:
  lines = [
      f'Base commit: `{result.base_commit[:12]}` | Head: `{result.head_ref}`',
      f'Changed files: {len(result.changed_files)} | Areas: ' +
      (', '.join(result.areas) if result.areas else 'none'),
      f'New default-false flags: ' +
      (', '.join(result.new_flags) if result.new_flags else 'none'),
      f'x64 C++ change-based eligible: {result.has_x64_d8_cpp} | '
      f'arm64 C++: {result.has_arm64_cpp} | CSA/Torque: '
      f'{result.has_csa_or_torque}',
      f'Target compatibility: --bundle-reprl={result.supports_bundle_reprl}, '
      f'v8_redline={result.supports_v8_redline}',
  ]
  if result.has_only_non_runtime:
    lines.append(
        '\nWARNING: Only non-runtime files (tests/docs/tools) changed.')
  if (result.unsupported_arches and not result.has_x64_d8_cpp and
      not result.has_arm64_cpp and not result.has_csa_or_torque):
    lines.append('\nWARNING: Only non-x64 architecture files changed: ' +
                 ', '.join(result.unsupported_arches))
  lines.append('\n### Suggested V8 Context Questions')
  for idx, q in enumerate(result.v8_questions, 1):
    lines.append(f'{idx}. {q}')
  lines.append('\n### Recommended Fuzzing Arms')
  if not result.arms:
    lines.append(
        '(No runtime code changed. Confirm target area before starting.)')
  else:
    lines.append(
        '| Arm | Build Variant | Coverage | Builtins Cov | Change-Based | '
        'Preset | Workers | Duration | Timeout | Extra d8 Flags | Seeds |')
    lines.append('|---|---|---|---|---|---|---|---|---|---|---|')
    for a in result.arms:
      dur = f'{a.duration_sec // 3600}h' if a.duration_sec else 'unlimited'
      lines.append(f'| {a.name} | {a.build_variant} | `{a.coverage_mode}` | '
                   f'{a.builtins_coverage} | {a.change_based} | `{a.preset}` | '
                   f'{a.jobs} | {dur} | {a.timeout_ms} ms | '
                   f'`{" ".join(a.d8_extra_args)}` | `{a.seeds}` |')
  return '\n'.join(lines)


def main(argv: list[str]) -> int:
  parser = argparse.ArgumentParser(
      description='Analyze a V8 CL/branch diff against origin/main.')
  parser.add_argument('--repo', default='.', help='Path to V8 git checkout')
  parser.add_argument('--base', default='origin/main', help='Base git ref')
  parser.add_argument('--head', default='HEAD', help='Target branch/commit')
  parser.add_argument(
      '--include-worktree',
      action='store_true',
      help='Include uncommitted working-tree modifications in the diff')
  parser.add_argument(
      '--vcpus',
      type=int,
      default=32,
      help='Host vCPU count used to size worker allocations')
  parser.add_argument(
      '--ram-gb',
      type=int,
      default=0,
      help='Host RAM in GB used to cap ASan workers (0 = uncapped)')
  parser.add_argument(
      '--duration-sec',
      type=int,
      default=DEFAULT_DURATION_SEC,
      help='Default run duration in seconds (default 43200 = 12h, 0 = none)')
  parser.add_argument(
      '--resize-plan',
      default='',
      help='Resize worker counts in an existing plan.json in place')
  parser.add_argument('--json', action='store_true', help='Output JSON')
  args = parser.parse_args(argv)

  try:
    if args.resize_plan:
      plan = resize_plan_file(args.resize_plan, args.vcpus, ram_gb=args.ram_gb)
      print(json.dumps(plan, indent=2))
      return 0
    result = analyze(
        args.repo,
        args.base,
        args.head,
        args.include_worktree,
        args.vcpus,
        ram_gb=args.ram_gb,
        duration_sec=args.duration_sec,
    )
  except (OSError, RuntimeError, ValueError) as err:
    print(f'Error: {err}', file=sys.stderr)
    return 1

  if args.json:
    payload = asdict(result)
    print(json.dumps(payload, indent=2))
  else:
    print(format_markdown(result))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
