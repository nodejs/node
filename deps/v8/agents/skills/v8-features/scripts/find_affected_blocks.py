#! /usr/bin/python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import subprocess
import os
import sys
import argparse
import concurrent.futures


def run_command(*args: str) -> str:
  git_process = subprocess.run(args, capture_output=True, text=True)
  if git_process.returncode == 0:
    return git_process.stdout.rstrip('\n')
  else:
    raise Exception(
        'Failed to run command {0} (return code {1})'.format(
            ' '.join(git_process.args), git_process.returncode),
        git_process.stderr)


class FileBlocks:

  class Block:
    file: str
    begin: int
    end: int

    def __init__(self, file: str, begin: int, end: int):
      self.file = file
      self.begin = begin
      self.end = end

    def __str__(self) -> str:
      if self.begin == self.end:
        return '{0}:{1}'.format(self.file, self.begin)
      else:
        return '{0}:{1}-{2}'.format(self.file, self.begin, self.end)

  porcelain_pattern = re.compile('([0-9a-f]{40}) ([0-9]+) ([0-9]+)( .*)?')

  commit_hashes: set[str]
  revision: str
  affected_files: set[str]
  affected_blocks: list[Block]

  def __init__(self,
               commit_hashes: set[str],
               revision: str = None,
               skip_file_predicate=None):
    self.commit_hashes = commit_hashes
    self.revision = revision
    self.skip_file_predicate = skip_file_predicate
    self.affected_files = set()
    self.skipped_files = set()
    self.min_commit_timestamp = None
    self.find_affected_files()
    self.affected_blocks = []
    self.find_affected_blocks()

  def files(self) -> set[str]:
    return self.affected_files

  def skipped(self) -> set[str]:
    return self.skipped_files

  def blocks(self) -> list[Block]:
    return self.affected_blocks

  def find_affected_files(self) -> None:
    if self.revision:
      try:
        run_command('git', 'rev-parse', '--verify',
                    f'{self.revision}^{{commit}}')
      except Exception as e:
        raise Exception(
            f'Revision {self.revision} is not present in the local git object '
            f'store (run `git fetch origin {self.revision}` first)') from e

    if not self.commit_hashes:
      return

    existing_at_rev = None
    if self.revision:
      tree_output = run_command('git', 'ls-tree', '-r', '--name-only',
                                self.revision)
      existing_at_rev = set(tree_output.split('\n')) if tree_output else set()

    commit_list = sorted(self.commit_hashes)
    try:
      ts_output = run_command('git', 'show', '-s', '--format=%ct', *commit_list)
      timestamps = [
          int(line.strip())
          for line in ts_output.split('\n')
          if line.strip().isdigit()
      ]
      if timestamps:
        # Subtract 1 day buffer to ensure git blame --since includes the oldest commit.
        self.min_commit_timestamp = min(timestamps) - 86400
    except Exception:
      self.min_commit_timestamp = None

    batch_size = 100
    candidate_files = set()
    for i in range(0, len(commit_list), batch_size):
      batch = commit_list[i:i + batch_size]
      output = run_command('git', 'show', '--name-only', '--pretty=format:',
                           *batch)
      if output:
        for line in output.split('\n'):
          line = line.strip()
          if line:
            candidate_files.add(line)

    for line in candidate_files:
      exists = False
      if self.revision:
        if existing_at_rev is not None:
          exists = line in existing_at_rev
        else:
          try:
            run_command('git', 'cat-file', '-e', f'{self.revision}:{line}')
            exists = True
          except Exception:
            pass
      elif os.path.isfile(line):
        exists = True

      if exists:
        if self.skip_file_predicate and self.skip_file_predicate(line):
          self.skipped_files.add(line)
        else:
          self.affected_files.add(line)

  def find_affected_blocks(self) -> None:

    def process_file(file: str) -> list[FileBlocks.Block]:
      line_numbers = set()
      cmd = ['git', 'blame', '--porcelain']
      if self.min_commit_timestamp is not None:
        cmd.append(f'--since=@{self.min_commit_timestamp}')
      if self.revision:
        cmd.append(self.revision)
      cmd.extend(['--', file])
      try:
        output = run_command(*cmd)
      except Exception:
        return []
      if output is not None and len(output) > 0:
        for line in output.split('\n'):
          if line[:40] in self.commit_hashes:
            match = FileBlocks.porcelain_pattern.fullmatch(line)
            if match is not None:
              line_numbers.add(int(match.group(3)))
      blocks = []
      block = None
      for line_number in sorted(line_numbers):
        if block is None:
          block = FileBlocks.Block(file, line_number, line_number)
        elif block.end + 1 == line_number:
          block.end = line_number
        else:
          blocks.append(block)
          block = FileBlocks.Block(file, line_number, line_number)
      if block is not None:
        blocks.append(block)
      return blocks

    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
      results = executor.map(process_file, sorted(self.affected_files))
      for blocks in results:
        self.affected_blocks.extend(blocks)


class HashResolver:
  commit_hashes: set[str]
  resolved_bugs: set[str]

  # Bulk feature-flags refactoring commit/bug to ignore when tracing flag history.
  BULK_FLAG_REFACTOR_PREFIXES = ('2628639aef5',)
  BULK_FLAG_REFACTOR_BUGS = {'448864127'}
  FLAG_FILES = [
      'src/flags/feature-flags.h',
      'src/wasm/wasm-feature-flags.h',
      'src/flags/flag-definitions.h',
  ]

  def __init__(self):
    self.commit_hashes = set()
    self.resolved_bugs = set()

  commit_message_bug_pattern = re.compile(
      '(?:https?://crbug\\.com/(?:v8/|chromium/)?|((v8|chromium|crbug|b)(:|/)))?([0-9]+)'
  )
  commit_hash_pattern = re.compile('[0-9a-fA-F]{7,40}')
  pure_digits_pattern = re.compile('^[0-9]+$')
  change_id_pattern = re.compile('I([0-9a-fA-F]{40})')
  bug_ref_patterns = [
      re.compile('b/([0-9]+)'),
      re.compile('b:([0-9]+)'),
      re.compile('v8:([0-9]+)'),
      re.compile('chromium:([0-9]+)'),
      re.compile(
          '(?:https?://)?crbug\\.com/(?:v8/|chromium/)?([0-9]+)([\\?#].*)?'),
      re.compile('https://b.corp.google.com/([0-9]+)([\\?#].*)?'),
      re.compile('https://b.corp.google.com/issues/([0-9]+)([\\?#].*)?'),
      re.compile('https://g-issues.chromium.org/issues/([0-9]+)([\\?#].*)?'),
      re.compile('https://issues.chromium.org/issues/([0-9]+)([\\?#].*)?'),
      re.compile('https://issues.chromium.org/([0-9]+)([\\?#].*)?'),
      re.compile('https://buganizer.corp.google.com/([0-9]+)([\\?#].*)?'),
      re.compile('^([0-9]+)$')
  ]

  def hashes(self) -> set[str]:
    return self.commit_hashes

  def bugs(self) -> set[str]:
    return self.resolved_bugs

  @staticmethod
  def extract_bug_numbers_from_text(text: str) -> set[str]:
    bug_numbers = set()
    if not text:
      return bug_numbers
    for line in text.split('\n'):
      line = line.strip(' ')
      lower = line.lower()
      if lower.startswith('bug:'):
        line = line[4:]
      elif lower.startswith('fixed:'):
        line = line[6:]
      else:
        continue
      for bug_id in re.split(r'[,\s]+', line.strip()):
        if not bug_id:
          continue
        bug_match = HashResolver.commit_message_bug_pattern.fullmatch(
            bug_id.strip())
        if bug_match is not None:
          bug_numbers.add(bug_match.group(4))
    return bug_numbers

  @staticmethod
  def extract_bug_numbers(commit_hash: str) -> set[str]:
    output = run_command('git', 'log', '-n', '1', '--pretty=format:%B',
                         commit_hash, '--')
    return HashResolver.extract_bug_numbers_from_text(output)

  def _resolve_bug_ids(self, bug_ids: set[str]) -> set[str]:
    """Resolves multiple bug IDs in a single git log --all traversal."""
    pending = sorted(b for b in bug_ids if b and b not in self.resolved_bugs)
    if not pending:
      return set(bug_ids) & self.resolved_bugs

    grep_args = [
        f'--grep=^(Bug|Fixed):[[:space:]]*.*(chromium:|v8:|b:|b/)?{b}\\b'
        for b in pending
    ]
    output = run_command('git', 'log', '--all', '-E', '-i', *grep_args,
                         '--pretty=format:%H%x00%B%x1e')
    found_bugs = set()
    if output:
      pending_set = set(pending)
      for record in output.split('\x1e'):
        record = record.strip()
        if not record or '\x00' not in record:
          continue
        commit_hash, body = record.split('\x00', 1)
        commit_hash = commit_hash.strip()
        matched = HashResolver.extract_bug_numbers_from_text(body) & pending_set
        if matched:
          self.commit_hashes.add(commit_hash)
          found_bugs.update(matched)
    self.resolved_bugs.update(found_bugs)
    return (set(bug_ids) & self.resolved_bugs) | found_bugs

  def _resolve_bug_id(self, bug_id: str) -> bool:
    return bug_id in self._resolve_bug_ids({bug_id})

  @staticmethod
  def _match_commit_hash(reference: str) -> str | None:
    match = HashResolver.commit_hash_pattern.fullmatch(reference)
    if match is None:
      return None
    commit_hash = match.group(0).lower()
    try:
      output = run_command('git', 'log', '--pretty=format:%H', '-n', '1',
                           commit_hash, '--')
      if output:
        return output
    except Exception:
      pass
    return None

  def _try_resolve_commit_hash(self, reference: str) -> bool:
    resolved = self._match_commit_hash(reference)
    if resolved:
      self.commit_hashes.add(resolved)
      return True
    return False

  def _scan_feature_flag_history(
      self, feature_name: str) -> tuple[set[str], set[str]]:
    """Scans flag files for a feature name, returning (commit_hashes, bug_ids)."""
    norm = feature_name.strip().lstrip('-').replace('-', '_')
    if norm.startswith('experimental_'):
      norm = norm[len('experimental_'):]
    if norm.startswith('wasm_'):
      norm = norm[len('wasm_'):]

    flag_files = list(HashResolver.FLAG_FILES)
    if os.path.isfile('src/flags/feature-flags.h'):
      try:
        with open('src/flags/feature-flags.h', 'r', errors='replace') as f:
          ff_content = f.read()
        if re.search(rf'\bWASM_FEATURE\s*\(\s*{re.escape(norm)}\b', ff_content):
          flag_files = [
              'src/flags/feature-flags.h',
              'src/wasm/wasm-feature-flags.h',
          ]
        elif re.search(
            rf'\b(JS_FEATURE|INTERNAL_FEATURE)\s*\(\s*{re.escape(norm)}\b',
            ff_content):
          flag_files = [
              'src/flags/feature-flags.h',
              'src/flags/flag-definitions.h',
          ]
      except Exception:
        pass

    output = run_command('git', 'log', f'-G\\b{re.escape(norm)}\\b',
                         '--pretty=format:%H%x00%B%x1e', '--', *flag_files)
    commits = set()
    bugs = set()
    if output:
      for record in output.split('\x1e'):
        record = record.strip()
        if not record or '\x00' not in record:
          continue
        commit_hash, body = record.split('\x00', 1)
        commit_hash = commit_hash.strip()
        if commit_hash.startswith(HashResolver.BULK_FLAG_REFACTOR_PREFIXES):
          continue
        commits.add(commit_hash)
        for bug_id in HashResolver.extract_bug_numbers_from_text(body):
          if bug_id not in HashResolver.BULK_FLAG_REFACTOR_BUGS:
            bugs.add(bug_id)
    if not commits:
      raise Exception(f'Could not find flag history for feature {feature_name}')
    return commits, bugs

  def add_feature(self, feature_name: str) -> None:
    """Resolves a feature flag name across flag history and related bugs."""
    commits, bugs = self._scan_feature_flag_history(feature_name)
    self.commit_hashes.update(commits)
    if bugs:
      self._resolve_bug_ids(bugs)

  @staticmethod
  def _scan_grep(pattern: str) -> set[str]:
    """Finds commits whose commit message matches the given regex pattern."""
    output = run_command('git', 'log', '--all', f'--grep={pattern}',
                         '--pretty=format:%H')
    commits = {line.strip() for line in output.split('\n') if line.strip()
              } if output else set()
    if not commits:
      raise Exception(f'Could not find commits matching --grep={pattern}')
    return commits

  def add_grep(self, pattern: str) -> None:
    """Resolves commits whose commit message matches the given regex pattern."""
    self.commit_hashes.update(self._scan_grep(pattern))

  def _resolve_non_bug_reference(self,
                                 reference: str) -> tuple[set[str], set[str]]:
    """Resolves a non-bug reference (Change-Id, commit hash, or feature name) without mutating self."""
    # 1. Check for Gerrit Change-Ids first (I<40 hex chars>)
    match = HashResolver.change_id_pattern.fullmatch(reference)
    if match is not None:
      output = run_command(
          'git', 'log', '--all',
          '--grep=Change-Id: I{0}'.format(match.group(1).lower()),
          '--pretty=format:%H', '-n', '1')
      if output is not None and len(output) > 0:
        return {output}, set()
      raise Exception('Could not find hash for change id I{0}'.format(
          match.group(1).lower()))

    # 2. Check for hexadecimal commit hashes
    resolved_hash = self._match_commit_hash(reference)
    if resolved_hash:
      return {resolved_hash}, set()

    # 3. Fallback: try resolving as a feature flag name
    try:
      return self._scan_feature_flag_history(reference)
    except Exception:
      pass

    raise Exception('Could not understand {0}'.format(reference))

  def resolve_all(self, references=None, features=None, greps=None) -> None:
    """Resolves all references, features, and grep patterns concurrently."""
    references = list(references or [])
    features = list(features or [])
    greps = list(greps or [])

    explicit_bugs = set()
    other_refs = []
    for ref in references:
      if HashResolver.pure_digits_pattern.fullmatch(ref):
        explicit_bugs.add(ref)
        continue
      matched_bug = None
      for pattern in HashResolver.bug_ref_patterns:
        m = pattern.fullmatch(ref)
        if m is not None:
          matched_bug = m.group(1)
          break
      if matched_bug is not None:
        explicit_bugs.add(matched_bug)
      else:
        other_refs.append(ref)

    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
      feat_futures = {
          executor.submit(self._scan_feature_flag_history, f): f
          for f in features
      }
      grep_futures = {executor.submit(self._scan_grep, g): g for g in greps}
      ref_futures = {
          executor.submit(self._resolve_non_bug_reference, r): r
          for r in other_refs
      }

      discovered_bugs = set(explicit_bugs)
      for fut in concurrent.futures.as_completed(feat_futures):
        commits, bugs = fut.result()
        self.commit_hashes.update(commits)
        discovered_bugs.update(bugs)
      for fut in concurrent.futures.as_completed(grep_futures):
        self.commit_hashes.update(fut.result())
      for fut in concurrent.futures.as_completed(ref_futures):
        commits, bugs = fut.result()
        self.commit_hashes.update(commits)
        discovered_bugs.update(bugs)

    if discovered_bugs:
      found_bugs = self._resolve_bug_ids(discovered_bugs)
      for b in explicit_bugs - found_bugs:
        if HashResolver.pure_digits_pattern.fullmatch(
            b) and self._try_resolve_commit_hash(b):
          continue
        raise Exception(f'Could not find hashes for bug {b}')

  def add_reference(self, reference: str) -> None:
    # 1. If reference is purely digits, prioritize bug ID lookup before
    # treating it as a potential hex commit hash prefix.
    if HashResolver.pure_digits_pattern.fullmatch(reference):
      if self._resolve_bug_id(reference):
        return
      if self._try_resolve_commit_hash(reference):
        return
      raise Exception(f'Could not find hashes for bug {reference}')

    # 2. Check explicit bug URL / prefixed bug patterns (b/..., v8:..., etc.)
    for pattern in HashResolver.bug_ref_patterns:
      match = pattern.fullmatch(reference)
      if match is not None:
        bug_id = match.group(1)
        if self._resolve_bug_id(bug_id):
          return
        raise Exception(f'Could not find hashes for bug {bug_id}')

    # 3. Resolve Change-Id, commit hash, or feature flag name
    commits, bugs = self._resolve_non_bug_reference(reference)
    self.commit_hashes.update(commits)
    if bugs:
      self._resolve_bug_ids(bugs)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Find all code blocks affected by a list of references to CLs'
  )
  parser.add_argument(
      'reference',
      nargs='*',
      help='Git commit hash, Gerrit Change-Id, bug ID/URL, or feature flag name'
  )
  parser.add_argument(
      '--feature',
      '-f',
      action='append',
      default=[],
      help='Feature flag name (e.g. wide_arithmetic) to trace across flag files and bugs'
  )
  parser.add_argument(
      '--grep',
      '-g',
      action='append',
      default=[],
      help='Commit message regex/substring (e.g. "\\[wasm-wide-arith\\]") to include'
  )
  parser.add_argument(
      '--hashes-only',
      '-H',
      action="store_true",
      help='only output Git commit hashes of all the referenced CLs')
  parser.add_argument(
      '--revision',
      '-r',
      help='Git revision to blame against (defaults to HEAD)')
  args = parser.parse_args()
  if not args.reference and not args.feature and not args.grep:
    parser.error('At least one reference, --feature, or --grep is required.')
  try:
    hash_resolver = HashResolver()
    hash_resolver.resolve_all(args.reference, args.feature, args.grep)
    if args.hashes_only:
      for hash in sorted(hash_resolver.hashes()):
        print(hash)
    else:
      blocks = FileBlocks(hash_resolver.hashes(), revision=args.revision)
      for block in blocks.blocks():
        print(block)
  except Exception as exception:
    print('ERROR - {}'.format(exception.args[0]), file=sys.stderr)
    for arg in exception.args[1:]:
      for line in arg.rstrip('\n').split('\n'):
        print('  {}'.format(line), file=sys.stderr)
    sys.exit(1)
