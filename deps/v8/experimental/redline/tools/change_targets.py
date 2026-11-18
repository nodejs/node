#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Computes the target edges of a change for Redline's change-based fuzzing.

The target edges are the coverage edges of d8 that belong to the lines that a
diff changed (line targets), and the other edges of the functions that contain
these lines (function targets). Redline watches them with hit count buckets
and gives them a large weight in the rarity selection (redline fuzz
--targets=FILE).

Only edges that can run without a failed check are targets. The script
removes the edges of check reporting and library code (by their source
frames), and the edges of blocks that only lead to a fatal error (by the
machine code, with llvm-objdump).

The focus is the code of the directories of the changed files (for example
src/maglev/). Redline keeps a new program only if it reaches new coverage in
the focus, in the optimized code features, or on a target.

The script needs the edge map of the d8 build (tools/edge_map.py; it runs it
when the map is missing).

Usage:
  change_targets.py --d8=out/rl_counters/d8 [--base=origin/main]
                    [--head=worktree|REV] [--out=targets.json]
                    [--focus=auto|none|PREFIX,...]
"""

import argparse
import array
import bisect
import concurrent.futures
import gzip
import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import edge_map  # pylint: disable=wrong-import-position

SOURCE_DIRS = ('src/', 'include/')
# Frames in these files belong to the code that reports a failed CHECK or
# DCHECK, or to library code inlined into V8 functions. Such edges run only
# when a check fails, or they do not belong to the change, so they are not
# targets.
NOISE_FILES = ('src/base/logging.h', 'src/base/logging.cc', 'third_party/',
               'buildtools/')
NOISE_FUNCTIONS = ('V8_Fatal', 'V8_Dcheck', 'MakeCheckOpString',
                   'PrintCheckOperand', 'FatalProcessOutOfMemory')


def is_noise(frames):
  """True if the innermost frame of an edge is check reporting or library
  code."""
  if not frames:
    return True
  file_name, line, function = frames[0]
  return (line == 0 or file_name.startswith(NOISE_FILES) or
          any(name in function for name in NOISE_FUNCTIONS))


DEFAULT_OBJDUMP = os.path.join(edge_map.V8_ROOT, 'third_party', 'llvm-build',
                               'Release+Asserts', 'bin', 'llvm-objdump')
# A call to one of these functions reports a failed check or aborts.
FATAL_CALLEES = re.compile(r'V8_Fatal|V8_Dcheck|MakeCheckOpString|'
                           r'FatalProcessOutOfMemory|\babort\b|__assert_fail|'
                           r'__libcpp_verbose_abort|__ubsan_handle')
# Instructions that end a block after a call that does not return.
TRAPS = ('int3', 'ud1', 'ud1l', 'ud2')
INSTRUCTION = re.compile(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$')
CALLEE = re.compile(r'<([^>]+)>')
BRANCH_TARGET = re.compile(r'^0x([0-9a-f]+)')


def parse_disassembly(text):
  """Returns [(address, mnemonic, callee or '', branch target or None)]."""
  result = []
  for line in text.splitlines():
    match = INSTRUCTION.match(line)
    if not match:
      continue
    operands = match.group(3)
    callee = CALLEE.search(operands)
    target = BRANCH_TARGET.match(operands.strip())
    result.append((int(match.group(1),
                       16), match.group(2), callee.group(1) if callee else '',
                   int(target.group(1), 16) if target else None))
  return result


def leads_to_fatal(instructions, addresses, pc, limit=48):
  """True if the block at `pc` only leads to a fatal error.

  The walk follows the straight-line code and the unconditional jumps. It
  stops at the first conditional branch or return (the block is live), or at
  a call to a fatal function or a call that does not return (the block is
  fatal).
  """
  i = bisect.bisect_left(addresses, pc)
  if i >= len(addresses) or addresses[i] != pc:
    return False
  for _ in range(limit):
    if i >= len(instructions):
      return False
    _, mnemonic, callee, target = instructions[i]
    if mnemonic.startswith('ret'):
      return False
    if mnemonic.startswith('jmp'):
      if FATAL_CALLEES.search(callee):
        return True
      if target is None:
        return False
      i = bisect.bisect_left(addresses, target)
      if i >= len(addresses) or addresses[i] != target:
        return False
      continue
    if mnemonic.startswith('j'):
      return False
    if mnemonic in TRAPS:
      return True
    if mnemonic.startswith('call'):
      if FATAL_CALLEES.search(callee):
        return True
      if i + 1 < len(instructions) and instructions[i + 1][1] in TRAPS:
        return True
    i += 1
  return False


def fatal_edges(d8, edges, objdump=DEFAULT_OBJDUMP, build_id=None):
  """Returns the subset of `edges` whose block only leads to a fatal error.

  Returns an empty set if llvm-objdump is missing or if the PC table does not
  belong to the build `build_id`.
  """
  if not edges or not os.path.exists(objdump):
    return set()
  table_id, table = edge_map.dump_pc_table(d8)
  if build_id and table_id != build_id:
    return set()
  pcs = {edge: int(table[edge - 1], 16) for edge in edges if edge <= len(table)}
  # One disassembly per cluster of addresses.
  ranges = []
  for pc in sorted(pcs.values()):
    if ranges and pc - ranges[-1][1] < 4096:
      ranges[-1][1] = pc + 512
    else:
      ranges.append([pc, pc + 512])

  def disassemble(bounds):
    return parse_disassembly(
        subprocess.run([
            objdump, '-d', '--no-show-raw-insn',
            '--start-address=0x%x' % bounds[0],
            '--stop-address=0x%x' % bounds[1], d8
        ],
                       capture_output=True,
                       text=True,
                       check=True).stdout)

  with concurrent.futures.ThreadPoolExecutor(
      max_workers=os.cpu_count() or 4) as pool:
    parts = list(pool.map(disassemble, ranges))
  instructions = sorted({i[0]: i for part in parts for i in part}.values())
  addresses = [i[0] for i in instructions]
  return set(edge for edge, pc in pcs.items()
             if leads_to_fatal(instructions, addresses, pc))


def focus_prefixes(changed, spec):
  """Returns the path prefixes of the focus.

  `spec` is 'auto' (the first two components of the changed files, for
  example src/maglev/), 'none', or a comma-separated list of prefixes.
  """
  if spec == 'none':
    return []
  if spec != 'auto':
    return sorted(p for p in spec.split(',') if p)
  prefixes = set()
  for file_name in changed:
    parts = file_name.split('/')
    if len(parts) > 2:
      prefixes.add('/'.join(parts[:2]) + '/')
  return sorted(prefixes)


class FocusCollector:
  """Collects the focus edges while the edge map is read once.

  An edge is in the focus if one of its frames is in a file under a focus
  prefix, or if its function has such an edge (for example, code that a
  function of the focus inlines from a header).
  """

  def __init__(self, prefixes):
    self.prefixes = tuple(prefixes)
    self.function_ids = {}
    self.edges = array.array('I')
    self.edge_function = array.array('I')
    self.in_prefix = bytearray()

  def wrap(self, edges):
    """Yields `edges` unchanged and records them."""
    for edge, frames in edges:
      if self.prefixes:
        function = frames[-1][2] if frames else ''
        self.edges.append(edge)
        self.edge_function.append(
            self.function_ids.setdefault(function, len(self.function_ids)))
        self.in_prefix.append(
            any(f.startswith(self.prefixes) for f, _, _ in frames))
      yield edge, frames

  def ranges(self):
    """Returns the focus edges as sorted, merged [first, last] ranges."""
    focus_functions = set(
        fid for fid, inside in zip(self.edge_function, self.in_prefix)
        if inside)
    # The empty function name is not a function.
    focus_functions.discard(self.function_ids.get(''))
    result = []
    for edge, fid, inside in zip(self.edges, self.edge_function,
                                 self.in_prefix):
      if not inside and fid not in focus_functions:
        continue
      if result and result[-1][1] == edge - 1:
        result[-1][1] = edge
      else:
        result.append([edge, edge])
    return result


SOURCE_SUFFIXES = ('.cc', '.h', '.inl')
HUNK = re.compile(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@')
NEW_FLAG = re.compile(r'^\+\s*DEFINE_BOOL\(\s*([a-z0-9_]+)\s*,\s*false\b')


def git(repo, *args):
  return subprocess.run(
      ['git', '-C', repo] + list(args),
      capture_output=True,
      text=True,
      check=True).stdout


def parse_diff(text):
  """Returns ({file: sorted changed lines}, {file: [hunk sizes]}, [flags]).

  For a hunk that only removes lines, the line before and the line after the
  removal count as changed.
  """
  changed = {}
  hunks = {}
  flags = []
  current = None
  for line in text.splitlines():
    if line.startswith('+++ '):
      path = line[4:]
      current = path[2:] if path.startswith('b/') else None
      continue
    if current is None:
      continue
    if current == 'src/flags/flag-definitions.h':
      match = NEW_FLAG.match(line)
      if match:
        flags.append('--' + match.group(1).replace('_', '-'))
    match = HUNK.match(line)
    if not match:
      continue
    start = int(match.group(1))
    count = int(match.group(2)) if match.group(2) is not None else 1
    lines = changed.setdefault(current, set())
    if count == 0:
      lines.update(n for n in (start, start + 1) if n > 0)
      hunks.setdefault(current, []).append(1)
    else:
      lines.update(range(start, start + count))
      hunks.setdefault(current, []).append(count)
  keep = lambda f: f.startswith(SOURCE_DIRS) and f.endswith(SOURCE_SUFFIXES)
  changed = {f: sorted(v) for f, v in changed.items() if keep(f)}
  hunks = {f: v for f, v in hunks.items() if keep(f)}
  return changed, hunks, flags


def read_edge_map(path):
  """Yields (edge, frames) of an edge map."""
  with gzip.open(path, 'rt') as f:
    f.readline()
    for line in f:
      entry = json.loads(line)
      yield entry['e'], entry['f']


def compute_targets(edges,
                    changed,
                    line_weight=1.0,
                    function_weight=0.3,
                    max_targets=None,
                    near_distance=2):
  """Returns (targets, functions, mapped lines).

  `edges` is an iterable of (edge, frames). `targets` is a list of
  {"edge", "w", "file", "line", "function"}, line targets first.

  A changed line without an edge of its own (for example, a line in a block
  that the compiler merged with its neighbour) takes the edges of the same
  file within `near_distance` lines as line targets.
  """
  changed_sets = {f: set(v) for f, v in changed.items()}
  line_targets = []
  functions = set()
  mapped = {}
  by_function = {}
  # Edges of the changed files that are not on a changed line, for the near
  # pass: (file, line) -> [(edge, frames)].
  candidates = {}
  noise = 0
  for edge, frames in edges:
    if is_noise(frames):
      if any(f in changed_sets for f, _, _ in frames):
        noise += 1
      continue
    hit = None
    for file_name, line, function in frames:
      if line in changed_sets.get(file_name, ()):
        hit = (file_name, line, function)
        break
    if hit is not None:
      line_targets.append({
          'edge': edge,
          'w': line_weight,
          'file': hit[0],
          'line': hit[1],
          'function': hit[2]
      })
      mapped.setdefault(hit[0], set()).add(hit[1])
      for _, _, function in frames:
        if function:
          functions.add(function)
      continue
    for file_name, line, _ in frames:
      if file_name in changed_sets:
        candidates.setdefault((file_name, line), []).append((edge, frames))
    if frames:
      function = frames[-1][2]
      if function:
        by_function.setdefault(function, []).append((edge, frames[0]))
  # Near pass for the changed lines without an edge.
  taken = set(t['edge'] for t in line_targets)
  for file_name, lines in changed_sets.items():
    for line in sorted(lines - mapped.get(file_name, set())):
      for distance in range(1, near_distance + 1):
        for near in (line - distance, line + distance):
          for edge, frames in candidates.get((file_name, near), []):
            if edge in taken:
              continue
            taken.add(edge)
            line_targets.append({
                'edge': edge,
                'w': line_weight,
                'file': file_name,
                'line': near,
                'function': frames[-1][2] if frames else ''
            })
            for _, _, function in frames:
              if function:
                functions.add(function)
  function_targets = []
  for function in sorted(functions):
    for edge, (file_name, line, _) in by_function.get(function, []):
      if edge in taken:
        continue
      function_targets.append({
          'edge': edge,
          'w': function_weight,
          'file': file_name,
          'line': line,
          'function': function
      })
  targets = line_targets + function_targets
  if max_targets is not None and len(targets) > max_targets:
    targets = targets[:max_targets]
  compute_targets.noise_edges = noise
  return targets, sorted(functions), mapped


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
  parser.add_argument('--d8', required=True)
  parser.add_argument('--edge-map')
  parser.add_argument('--base', default='origin/main')
  parser.add_argument('--head', default='worktree')
  parser.add_argument('--repo', default=edge_map.V8_ROOT)
  parser.add_argument('--out', default='targets.json')
  parser.add_argument('--line-weight', type=float, default=1.0)
  parser.add_argument('--function-weight', type=float, default=0.3)
  parser.add_argument('--max-fraction', type=float, default=0.05)
  parser.add_argument('--near-distance', type=int, default=2)
  parser.add_argument('--focus', default='auto')
  parser.add_argument('--objdump', default=DEFAULT_OBJDUMP)
  parser.add_argument(
      '--keep-fatal',
      action='store_true',
      help='keep the target edges that only lead to a fatal error')
  args = parser.parse_args(argv)

  map_path = args.edge_map or edge_map.default_output(args.d8)
  if edge_map.main(['--d8', args.d8, '--out', map_path]) != 0:
    return 1
  header = edge_map.read_header(map_path)

  base_commit = git(args.repo, 'merge-base', args.base,
                    'HEAD' if args.head == 'worktree' else args.head).strip()
  diff_args = ['diff', '--unified=0', '-w', '-M', base_commit]
  if args.head != 'worktree':
    diff_args.append(args.head)
  changed, hunks, flags = parse_diff(git(args.repo, *diff_args))
  if not changed:
    sys.exit('The diff changes no file under src/ or include/')

  code_edges = header['edges']
  max_targets = int(args.max_fraction * code_edges)
  focus = FocusCollector(focus_prefixes(changed, args.focus))
  targets, functions, mapped = compute_targets(
      focus.wrap(read_edge_map(map_path)), changed, args.line_weight,
      args.function_weight, max_targets, args.near_distance)
  fatal = set()
  if not args.keep_fatal:
    fatal = fatal_edges(args.d8, [t['edge'] for t in targets], args.objdump,
                        header.get('build_id'))
    targets = [t for t in targets if t['edge'] not in fatal]
  focus_ranges = focus.ranges()
  unmapped = {}
  for file_name, lines in changed.items():
    missing = [n for n in lines if n not in mapped.get(file_name, set())]
    if missing:
      unmapped[file_name] = missing
  line_targets = sum(1 for t in targets if t['w'] == args.line_weight)
  result = {
      'version': 1,
      'd8': os.path.abspath(args.d8),
      'build_id': header.get('build_id', ''),
      'code_edges': code_edges,
      'base': args.base,
      'base_commit': base_commit,
      'head': args.head,
      'generated_at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
      'changed_lines': changed,
      'functions': functions,
      'targets': targets,
      'focus': {
          'prefixes': list(focus.prefixes),
          'ranges': focus_ranges,
      },
      'unmapped_lines': unmapped,
      'flags': flags,
      'stats': {
          'changed_lines':
              sum(len(v) for v in changed.values()),
          'hunks':
              sum(len(v) for v in hunks.values()),
          'line_targets':
              line_targets,
          'function_targets':
              len(targets) - line_targets,
          'check_or_library_edges_excluded':
              getattr(compute_targets, 'noise_edges', 0),
          'fatal_path_edges_excluded':
              len(fatal),
          'focus_edges':
              sum(last - first + 1 for first, last in focus_ranges),
          'capped':
              len(targets) >= max_targets,
      },
  }
  with open(args.out, 'w') as f:
    json.dump(result, f, indent=1)
  print('%s: %d changed lines, %d line targets, %d function targets%s, '
        '%d fatal-path edges excluded, %d focus edges (%s)' %
        (args.out, result['stats']['changed_lines'], line_targets,
         len(targets) - line_targets,
         ' (capped)' if result['stats']['capped'] else '', len(fatal),
         result['stats']['focus_edges'], ','.join(focus.prefixes) or 'none'))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
