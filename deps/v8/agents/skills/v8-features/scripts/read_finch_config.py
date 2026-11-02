#! /usr/bin/python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads active V8/Wasm Finch study configs from google3 and Chromium's fieldtrial_testing_config.json."""

import argparse
import base64
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import requests

GOOGLE3_FINCH_GCL_DIR = (
    '/google/src/files/head/depot/google3/googledata/googleclient/chrome/finch/gcl_studies'
)
FIELDTRIAL_TESTING_CONFIG_URL = (
    'https://chromium.googlesource.com/chromium/src.git/+/refs/heads/main/'
    'testing/variations/fieldtrial_testing_config.json')

FEATURE_DECL_RE = re.compile(
    r'\bFeature\s+([A-Za-z0-9_]+)\s*=\s*'
    r'(configs\.(enabled|disabled)_feature|\{\s*enabled\s*=\s*(true|false))')
ROLLOUT_HELPER_RE = re.compile(
    r'configs\.(?:BinaryFeatureGroups|StandardFeatureRollout(?:WithPreperiods)?)'
    r'\s*\([^)]*?\'([^\']+)\'')
STUDY_NAME_RE = re.compile(
    r'Study\s+study\s*=\s*\{[^}]*?\bname\s*=\s*\'([^\']+)\'', re.DOTALL)
ACTIVE_STAGE_RE = re.compile(r'^\s*(?:RolloutStage\s+)?active\s*=\s*([^\n{]+)',
                             re.MULTILINE)
CHANNEL_CONFIG_RE = re.compile(
    r'^\s*Config\s+(canary_dev|canary|dev|beta|stable)\s*=', re.MULTILINE)


def is_v8_or_wasm_feature(name: str) -> bool:
  norm = name[1:] if name.startswith('k') and name[1:2].isupper() else name
  return norm.startswith(('V8', 'WebAssembly', 'Wasm', 'JavaScript'))


def format_v8_flag(feature_name: str) -> str:
  if feature_name.startswith('V8Flag_'):
    raw = feature_name[len('V8Flag_'):]
    return f'--{raw.lower().replace("_", "-")} ({feature_name})'
  return feature_name


class Google3FinchGclScanner:
  """Scans actual Finch GCL study files in google3."""

  SKIP_DIRS = {'launched', 'autoconverted', 'legacy', 'metrics', 'templates'}

  def __init__(self,
               gcl_dir: str = GOOGLE3_FINCH_GCL_DIR,
               include_launched: bool = False):
    self.gcl_dir = gcl_dir
    self.include_launched = include_launched
    self.available = os.path.isdir(gcl_dir)
    # Maps feature_or_flag -> list of (study_name, rel_path, state_str, stage_str)
    self.entries: dict[str, list[tuple[str, str, str, str]]] = {}
    if self.available:
      self._scan()

  def _discover_candidate_files(self) -> set[str]:
    candidates = set()
    if shutil.which('cs'):
      excl = '' if self.include_launched else '-f:/(launched|autoconverted|legacy|ukm_sampling)/ '
      query = (
          f'f:^//depot/google3/googledata/googleclient/chrome/finch/gcl_studies/ '
          f'{excl}'
          f'(V8Flag_|WebAssembly|Feature\\s+V8|BinaryFeatureGroups.*(V8|Wasm|WebAssembly|JavaScript)|'
          f'StandardFeatureRollout.*(V8|Wasm|WebAssembly|JavaScript))')
      try:
        res = subprocess.run(
            ['cs', '--max_num_results=500', '-l', query],
            capture_output=True,
            text=True,
            timeout=8,
        )
        if res.returncode == 0 and res.stdout:
          for line in res.stdout.splitlines():
            line = line.strip()
            if line.endswith('.gcl') and os.path.isfile(line):
              candidates.add(line)
      except Exception:
        pass

    if candidates:
      for sub in ('', 'performance_teams', 'web_platform'):
        d = os.path.join(self.gcl_dir, sub) if sub else self.gcl_dir
        if not os.path.isdir(d):
          continue
        for fn in os.listdir(d):
          if fn.endswith('.gcl') and any(
              k in fn.lower()
              for k in ('v8', 'wasm', 'webassembly', 'javascript')):
            candidates.add(os.path.join(d, fn))
      return candidates

    skip_dirs = ({'autoconverted', 'legacy', 'metrics', 'templates'}
                 if self.include_launched else self.SKIP_DIRS)
    for dirpath, dirnames, filenames in os.walk(self.gcl_dir):
      dirnames[:] = [d for d in dirnames if d not in skip_dirs]
      for fn in filenames:
        if fn.endswith('.gcl') and any(
            k in fn.lower()
            for k in ('v8', 'wasm', 'webassembly', 'javascript')):
          candidates.add(os.path.join(dirpath, fn))
    return candidates

  def _parse_gcl_file(self, path: str) -> list[tuple[str, str, str, str, str]]:
    try:
      with open(path, 'r', errors='replace') as f:
        text = f.read()
    except OSError:
      return []

    rel_path = os.path.relpath(path, self.gcl_dir)
    study_match = STUDY_NAME_RE.search(text)
    study_name = study_match.group(1) if study_match else os.path.splitext(
        os.path.basename(path))[0]

    active_stages = [
        s.strip() for s in ACTIVE_STAGE_RE.findall(text) if s.strip()
    ]
    channel_configs = CHANNEL_CONFIG_RE.findall(text)
    is_killswitch = bool(
        re.search(r'_group_templates\.disabled|Kill[Ss]witch', text) and
        not re.search(r'_group_templates\.enabled', text))

    if active_stages:
      stage_str = active_stages[-1]
      if stage_str.startswith('configs.LaunchStage'):
        stage_str = 'KillSwitch/LaunchStage' if is_killswitch else 'LaunchStage'
      elif stage_str.startswith('configs.MultiArmConfig'):
        stage_str = 'MultiArmConfig'
    elif channel_configs:
      stage_str = 'channels:' + ','.join(dict.fromkeys(channel_configs))
    else:
      stage_str = 'configured'

    if not self.include_launched and stage_str == 'LAUNCHED':
      return []

    feat_states: dict[str, set[str]] = {}
    for m in FEATURE_DECL_RE.finditer(text):
      feat = m.group(1)
      if not is_v8_or_wasm_feature(feat):
        continue
      st = 'enabled' if (m.group(3) == 'enabled' or
                         m.group(4) == 'true') else 'disabled'
      feat_states.setdefault(feat, set()).add(st)

    for m in ROLLOUT_HELPER_RE.finditer(text):
      feat = m.group(1)
      if not is_v8_or_wasm_feature(feat):
        continue
      st = 'disabled' if is_killswitch else 'enabled'
      feat_states.setdefault(feat, set()).add(st)

    results = []
    for feat, states in feat_states.items():
      if 'enabled' in states and 'disabled' in states:
        state_str = 'enabled+control'
      elif 'enabled' in states:
        state_str = 'enabled'
      else:
        state_str = 'disabled (kill-switch)'
      results.append((feat, study_name, rel_path, state_str, stage_str))
    return results

  def _scan(self) -> None:
    files = sorted(self._discover_candidate_files())
    with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
      for file_results in executor.map(self._parse_gcl_file, files):
        for feat, study_name, rel_path, state_str, stage_str in file_results:
          self.entries.setdefault(feat, []).append(
              (study_name, rel_path, state_str, stage_str))


class FieldTrialConfig:
  """Parses Chromium's testing/variations/fieldtrial_testing_config.json."""

  def __init__(self, content: str):
    data = json.loads(content)
    # Maps feature_or_flag -> list of (study_name, exp_name, state, platforms)
    self.v8_flags: dict[str, list[tuple[str, str, str, list[str]]]] = {}
    self.base_features: dict[str, list[tuple[str, str, str, list[str]]]] = {}
    for study_name, config_array in data.items():
      for config in config_array:
        platforms = config.get('platforms', [])
        for experiment in config.get('experiments', []):
          exp_name = experiment.get('name', 'unnamed')
          for key, state in (
              ('enable_features', 'enabled'),
              ('disable_features', 'disabled'),
          ):
            for feature in experiment.get(key, []):
              if feature.startswith('V8Flag_'):
                self.v8_flags.setdefault(feature, []).append(
                    (study_name, exp_name, state, platforms))
              elif is_v8_or_wasm_feature(feature):
                self.base_features.setdefault(feature, []).append(
                    (study_name, exp_name, state, platforms))


def _format_fieldtrial_entries(entries: list[tuple[str, str, str, list[str]]],
                               include_disabled: bool) -> str | None:
  has_enabled = any(st == 'enabled' for _, _, st, _ in entries)
  if not has_enabled and not include_disabled:
    return None
  parts = sorted({f'{s}/{e} ({st})' for s, e, st, _ in entries})
  return ', '.join(parts)


def print_report(
    gcl_scanner: Google3FinchGclScanner | None,
    fieldtrial_config: FieldTrialConfig | None,
    filter_pattern: str | None = None,
    include_disabled: bool = False,
) -> None:
  regex = re.compile(filter_pattern, re.IGNORECASE) if filter_pattern else None

  if gcl_scanner and gcl_scanner.available:
    print(f'=== Google3 Finch Studies ({gcl_scanner.gcl_dir}) ===')
    printed_gcl = 0
    for feat in sorted(gcl_scanner.entries.keys()):
      items = gcl_scanner.entries[feat]
      has_enabled = any(st.startswith('enabled') for _, _, st, _ in items)
      if not has_enabled and not include_disabled:
        continue
      label = format_v8_flag(feat)
      studies = ', '.join(f'{study} [{rel}: {st}, stage={stage}]'
                          for study, rel, st, stage in items)
      line = f'{label} -> Finch GCL: {studies}'
      if regex and not regex.search(line):
        continue
      print(line)
      printed_gcl += 1
    if printed_gcl == 0:
      print('  (No matching active Finch GCL studies found)')
    print()

  if fieldtrial_config:
    print('=== Chromium FieldTrial Testing Config '
          '(testing/variations/fieldtrial_testing_config.json) ===')
    print(
        'Note: FieldTrial testing config is required before running a Finch trial, '
        'but presence here alone does not mean an active Finch rollout is running.'
    )
    printed_ft = 0
    all_ft = {}
    all_ft.update(fieldtrial_config.v8_flags)
    all_ft.update(fieldtrial_config.base_features)
    for feat in sorted(all_ft.keys()):
      summary = _format_fieldtrial_entries(all_ft[feat], include_disabled)
      if not summary:
        continue
      label = format_v8_flag(feat)
      line = f'{label} -> FieldTrial: {summary}'
      if regex and not regex.search(line):
        continue
      print(line)
      printed_ft += 1
    if printed_ft == 0:
      print('  (No matching FieldTrial testing config entries found)')


def fetch_fieldtrial_config(url: str) -> FieldTrialConfig:
  request = requests.get(url + '?format=TEXT', timeout=15)
  if request.status_code != 200:
    raise Exception(
        f'Could not load FieldTrial testing configuration from {url} '
        f'(HTTP {request.status_code})')
  content = base64.b64decode(request.text).decode('utf-8')
  return FieldTrialConfig(content)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description=(
          'Report active V8/Wasm Finch studies from google3 GCL configs '
          'and Chromium fieldtrial_testing_config.json.'))
  parser.add_argument(
      '--gcl-dir',
      default=GOOGLE3_FINCH_GCL_DIR,
      help='Path to google3 Finch GCL studies directory',
  )
  parser.add_argument(
      '--url',
      default=FIELDTRIAL_TESTING_CONFIG_URL,
      help='URL of Chromium fieldtrial_testing_config.json',
  )
  parser.add_argument(
      '--filter',
      '-f',
      help='Optional case-insensitive regex to filter reported flags/features',
  )
  parser.add_argument(
      '--include-disabled',
      action='store_true',
      help='Include disabled-only (kill-switch / control-only) entries',
  )
  parser.add_argument(
      '--include-launched',
      action='store_true',
      help='Include GCL studies marked LAUNCHED or in launched/ directory',
  )
  args = parser.parse_args()

  with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
    gcl_future = executor.submit(Google3FinchGclScanner, args.gcl_dir,
                                 args.include_launched)
    ft_future = executor.submit(fetch_fieldtrial_config, args.url)
    gcl_scanner = gcl_future.result()
    try:
      ft_config = ft_future.result()
    except Exception as e:
      print(f'ERROR - {e}', file=sys.stderr)
      if not gcl_scanner.available:
        sys.exit(1)
      ft_config = None

  print_report(
      gcl_scanner,
      ft_config,
      filter_pattern=args.filter,
      include_disabled=args.include_disabled,
  )
