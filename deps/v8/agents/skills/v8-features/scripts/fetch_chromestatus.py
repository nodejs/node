#! /usr/bin/python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import concurrent.futures
import json
import re
import sys
import requests

CHROMESTATUS_API_BASE = 'https://chromestatus.com/api/v0/features'
CHROMESTATUS_FEATURE_URL = 'https://chromestatus.com/feature/{}'
FEATURE_ID_PATTERN = re.compile(
    r'(?:chromestatus\.com|cr-status\.appspot\.com)/(?:api/v0/)?features?/(\d+)'
)
XSSI_PREFIX = ")]}'\n"


def parse_xssi_json(text: str):
  if text.startswith(")]}'"):
    text = text.split('\n', 1)[1] if '\n' in text else ''
  return json.loads(text)


def resolve_redirect(url: str, timeout: int = 10) -> str:
  try:
    resp = requests.head(
        url,
        allow_redirects=True,
        timeout=timeout,
        headers={'User-Agent': 'Mozilla/5.0'},
    )
    if resp.url and (resp.ok or resp.url != url):
      return resp.url
  except requests.RequestException:
    pass
  try:
    resp = requests.get(
        url,
        allow_redirects=True,
        stream=True,
        timeout=timeout,
        headers={'User-Agent': 'Mozilla/5.0'},
    )
    resp.close()
    return resp.url
  except requests.RequestException as e:
    print(f'ERROR - Could not resolve URL {url}: {e}', file=sys.stderr)
    return url


def fetch_feature_by_id(feature_id: str, timeout: int = 10) -> dict:
  url = f'{CHROMESTATUS_API_BASE}/{feature_id}'
  resp = requests.get(
      url, timeout=timeout, headers={'User-Agent': 'Mozilla/5.0'})
  if resp.status_code != 200:
    print(
        f'ERROR - Could not fetch Chromestatus feature {feature_id} '
        f'(HTTP {resp.status_code})',
        file=sys.stderr,
    )
    return {}
  return parse_xssi_json(resp.text)


def search_features(query: str, timeout: int = 10) -> list[dict]:
  # Normalize flag-like names (e.g. --wasm-wide-arithmetic or wide_arithmetic).
  normalized = query.lstrip('-')
  for prefix in ('experimental-wasm-', 'wasm-', 'experimental_wasm_', 'wasm_'):
    if normalized.lower().startswith(prefix):
      normalized = normalized[len(prefix):]
      break
  normalized = normalized.replace('_', ' ').replace('-', ' ').strip()

  queries_to_try = [query]
  if normalized and normalized.lower() != query.lower():
    queries_to_try.insert(0, normalized)

  seen_ids = set()
  results = []
  for q in queries_to_try:
    resp = requests.get(
        CHROMESTATUS_API_BASE,
        params={'q': q},
        timeout=timeout,
        headers={'User-Agent': 'Mozilla/5.0'},
    )
    if resp.status_code != 200:
      continue
    data = parse_xssi_json(resp.text)
    items = []
    for item in data.get('features', []):
      fid = item.get('id')
      if fid and fid not in seen_ids:
        seen_ids.add(fid)
        items.append((fid, item))
    if items:
      with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
        fut_map = {
            fid: executor.submit(
                fetch_feature_by_id, str(fid),
                timeout=timeout) for fid, _ in items
        }
        for fid, item in items:
          detail = fut_map[fid].result()
          results.append(detail or item)
    if results:
      break
  return results


def format_feature_markdown(f: dict) -> str:
  fid = f.get('id')
  name = f.get('name', 'Unknown')
  url = CHROMESTATUS_FEATURE_URL.format(fid) if fid else 'Unknown'
  chrome = f.get('browsers', {}).get('chrome', {})
  status = chrome.get('status', {}).get('text') or f.get('intent_stage')
  standards = f.get('standards', {})
  maturity = standards.get('maturity', {}).get('text') or standards.get(
      'maturity', {}).get('short_text')
  spec = f.get('spec_link') or standards.get('spec')
  bug = f.get('bug_url') or chrome.get('bug')
  launch_bug = f.get('launch_bug_url')
  owners = f.get('owner_emails') or chrome.get('owners') or []
  editors = f.get('editor_emails') or f.get('editors') or []
  cc_list = f.get('cc_emails') or f.get('cc_recipients') or []

  lines = [
      f'### {name} (ID: `{fid}`)',
      f'- **URL:** {url}',
      f'- **Summary:** {f.get("summary")}',
      f'- **Blink Component(s):** {", ".join(f.get("blink_components", []))}',
      f'- **Status:** {status} (Intent stage: {f.get("intent_stage")})',
      f'- **Feature Type:** {f.get("feature_type")}',
      f'- **Rollout Plan:** {f.get("rollout_plan_displayname")}',
      f'- **Flag / Finch Name:** flag=`{f.get("flag_name")}`, '
      f'finch=`{f.get("finch_name")}`',
      f'- **Owners:** {", ".join(owners) if owners else "None"}',
      f'- **Editors:** {", ".join(editors) if editors else "None"}',
      f'- **CC:** {", ".join(cc_list) if cc_list else "None"}',
      f'- **Tracking Bug:** {bug}',
      f'- **Launch Bug:** {launch_bug}',
      f'- **Spec Link:** {spec}',
      f'- **Standards Maturity:** {maturity}',
      f'- **Explainer Links:** {f.get("explainer_links") or []}',
      f'- **Doc Links:** {f.get("doc_links") or []}',
      f'- **TAG Review:** {f.get("tag_review")} '
      f'({f.get("tag_review_status")})',
      f'- **Security / Privacy Review:** '
      f'security={f.get("security_review_status")}, '
      f'privacy={f.get("privacy_review_status")}',
      f'- **Created / Updated:** '
      f'created={f.get("created", {}).get("when")} by '
      f'{f.get("created", {}).get("by")}, '
      f'updated={f.get("updated", {}).get("when")} by '
      f'{f.get("updated", {}).get("by")}',
  ]

  # Extract milestone / intent thread details from stages if populated.
  stage_summaries = []
  for st in f.get('stages', []):
    milestones = {
        k: st.get(k) for k in (
            'desktop_first',
            'android_first',
            'ios_first',
            'webview_first',
            'rollout_milestone',
        ) if st.get(k) is not None
    }
    intent_url = st.get('intent_thread_url')
    finch_url = st.get('finch_url')
    if milestones or intent_url or finch_url:
      stage_summaries.append(f'stage_type={st.get("stage_type")} '
                             f'milestones={milestones} '
                             f'intent_thread={intent_url} '
                             f'finch_url={finch_url}')
  if stage_summaries:
    lines.append('- **Populated Stages:** ' + '; '.join(stage_summaries))

  return '\n'.join(lines)


def main():
  parser = argparse.ArgumentParser(
      description=(
          'Search Chromestatus features, fetch feature metadata by ID/URL, '
          'or resolve search_web redirect URLs.'))
  parser.add_argument(
      'inputs',
      nargs='+',
      help=('Feature search query, V8 flag name (e.g. wide_arithmetic), '
            'Chromestatus feature ID/URL, or HTTP redirect URL.'),
  )
  parser.add_argument(
      '--json',
      action='store_true',
      help='Output raw JSON instead of formatted markdown summary.',
  )
  parser.add_argument(
      '--resolve-only',
      action='store_true',
      help='Only resolve redirect URLs and print their target URLs.',
  )
  args = parser.parse_args()

  features = []
  for item in args.inputs:
    item = item.strip()
    if item.startswith(('http://', 'https://')):
      match = FEATURE_ID_PATTERN.search(item) if not args.resolve_only else None
      if match:
        feat = fetch_feature_by_id(match.group(1))
        if feat:
          features.append(feat)
        continue
      resolved = resolve_redirect(item)
      match = FEATURE_ID_PATTERN.search(resolved)
      if args.resolve_only or not match:
        if match:
          canonical = CHROMESTATUS_FEATURE_URL.format(match.group(1))
          print(f'{item} -> {canonical} (raw: {resolved})')
        else:
          print(f'{item} -> {resolved}')
        if args.resolve_only:
          continue
      if match:
        feat = fetch_feature_by_id(match.group(1))
        if feat:
          features.append(feat)
    elif item.isdigit():
      feat = fetch_feature_by_id(item)
      if feat:
        features.append(feat)
    else:
      features.extend(search_features(item))

  if args.resolve_only:
    return

  if args.json:
    print(json.dumps(features, indent=2))
    return

  if not features:
    print('No matching Chromestatus entries found.')
    return

  for idx, feat in enumerate(features):
    if idx > 0:
      print('\n---\n')
    print(format_feature_markdown(feat))


if __name__ == '__main__':
  main()
