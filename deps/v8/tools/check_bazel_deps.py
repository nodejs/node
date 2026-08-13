#!/usr/bin/env python3

# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to check and optionally update bazel_dep versions in MODULE.bazel.

Can be run manually or set up as a cron job to monitor dependency updates.
Allows distinguishing between stable and pre-release (e.g., rc, beta) updates.
"""

import argparse
import json
import os
import re
import sys
import urllib.request
from urllib.error import HTTPError

BCR_URL = "https://raw.githubusercontent.com/bazelbuild/bazel-central-registry/main/modules/{name}/metadata.json"


def split_components(part_str):
  components = []
  for x in part_str.split('.'):
    if not x:
      continue
    if x.isdigit():
      components.append((0, int(x)))  # Compare numerically
    else:
      components.append((1, x))  # Compare alphabetically
  return components


def parse_version(version_str):
  # Splits version by hyphen to distinguish release and pre-release parts, matching Bzlmod/SemVer
  if '-' in version_str:
    release_str, prerelease_str = version_str.split('-', 1)
  else:
    release_str, prerelease_str = version_str, ""

  release_components = split_components(release_str)
  prerelease_components = split_components(prerelease_str)

  # Pre-release priority: 1 if stable (no pre-release), 0 if pre-release
  is_stable_val = 1 if not prerelease_str else 0

  return (release_components, is_stable_val, prerelease_components)


def is_stable(version_str):
  if '-' in version_str:
    pre_release_part = version_str.split('-', 1)[1].lower()
    for marker in [
        'rc', 'alpha', 'beta', 'dev', 'pre', 'preview', 'nightly', 'milestone',
        'm'
    ]:
      if re.search(r'\b' + marker + r'\d*\b', pre_release_part) or re.search(
          r'^' + marker + r'\d*', pre_release_part):
        return False
  return True


def get_bcr_versions(name):
  url = BCR_URL.format(name=name)
  try:
    req = urllib.request.Request(
        url, headers={'User-Agent': 'Mozilla/5.0 (Bazel Dependency Checker)'})
    with urllib.request.urlopen(req) as response:
      data = json.loads(response.read().decode())
      versions = data.get("versions", [])
      yanked = data.get("yanked_versions", {})

      # Filter out yanked versions
      valid_versions = [v for v in versions if v not in yanked]
      return valid_versions
  except HTTPError as e:
    if e.code == 404:
      return []
    raise
  except Exception as e:
    print(f"Error checking {name}: {e}", file=sys.stderr)
    return []


def update_dependency_version(content, name, new_version):
  # Match bazel_dep(name = "name", version = "old_version", ...)
  pattern = re.compile(
      r'(bazel_dep\s*\(\s*name\s*=\s*"' + re.escape(name) +
      r'"[^)]*version\s*=\s*")([^"]+)("[^)]*\))', re.DOTALL)
  if pattern.search(content):
    return pattern.sub(r'\g<1>' + new_version + r'\g<3>', content)

  # Match bazel_dep(version = "old_version", name = "name", ...)
  pattern_alt = re.compile(
      r'(bazel_dep\s*\(\s*version\s*=\s*")([^"]+)("[^)]*name\s*=\s*"' +
      re.escape(name) + r'"[^)]*\))', re.DOTALL)
  if pattern_alt.search(content):
    return pattern_alt.sub(r'\g<1>' + new_version + r'\g<3>', content)

  return content


def main():
  v8_base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  default_module_bazel = os.path.join(v8_base_dir, "MODULE.bazel")

  parser = argparse.ArgumentParser(
      description="Check and update MODULE.bazel dependency versions.")
  parser.add_argument(
      "--file", "-f", default=default_module_bazel, help="Path to MODULE.bazel file")
  parser.add_argument(
      "--update",
      "-u",
      action="store_true",
      help="Update MODULE.bazel in-place")
  parser.add_argument(
      "--pre",
      action="store_true",
      help="Include pre-releases (rc, alpha, beta) when updating")
  args = parser.parse_args()

  try:
    with open(args.file, "r") as f:
      content = f.read()
  except FileNotFoundError:
    print(f"Error: {args.file} not found.", file=sys.stderr)
    sys.exit(1)

  # Find all bazel_dep declarations
  dep_pattern = re.compile(
      r'bazel_dep\s*\(\s*name\s*=\s*"([^"]+)"\s*,\s*version\s*=\s*"([^"]+)"[^)]*\)'
  )

  deps = dep_pattern.findall(content)
  if not deps:
    print("No bazel_dep declarations found.")
    return

  # Header line
  print(
      f"{'Dependency':<20} | {'Current':<13} | {'Latest Stable':<15} | {'Latest (All)':<15} | Status"
  )
  print("-" * 85)

  updates = []
  for name, current_version in deps:
    all_versions = get_bcr_versions(name)
    if not all_versions:
      print(
          f"{name:<20} | {current_version:<13} | {'N/A':<15} | {'N/A':<15} | Not found in BCR or error"
      )
      continue

    all_versions.sort(key=parse_version)
    latest_all = all_versions[-1]

    stable_versions = [v for v in all_versions if is_stable(v)]
    latest_stable = stable_versions[-1] if stable_versions else latest_all

    status = "Up to date"
    needs_update = False
    target_version = latest_stable

    # Check if there is a newer stable version
    if parse_version(latest_stable) > parse_version(current_version):
      status = "STABLE UPDATE"
      needs_update = True
      target_version = latest_stable
    # Or check if we are allowing pre-releases and there is a newer pre-release
    elif args.pre and parse_version(latest_all) > parse_version(
        current_version):
      status = "PRE-RELEASE UPDATE"
      needs_update = True
      target_version = latest_all
    # If there is a newer pre-release but we are NOT updating to it
    elif parse_version(latest_all) > parse_version(current_version):
      status = "Pre-release available"

    # Flag if the current version itself is pre-release
    current_is_stable = is_stable(current_version)
    stable_indicator = "" if current_is_stable else " (Pre-release)"

    print(
        f"{name:<20} | {current_version + stable_indicator:<13} | {latest_stable:<15} | {latest_all:<15} | {status}"
    )

    if needs_update:
      updates.append((name, current_version, target_version))

  if updates and args.update:
    print("\nUpdating MODULE.bazel...")
    updated_content = content
    for name, _, target_version in updates:
      updated_content = update_dependency_version(updated_content, name,
                                                  target_version)

    with open(args.file, "w") as f:
      f.write(updated_content)

    print("Successfully updated versions in MODULE.bazel.")
  elif updates:
    update_type = "pre-release" if args.pre else "stable"
    print(
        f"\nFound {len(updates)} {update_type} update(s). Run with --update or -u to apply them automatically."
    )
  else:
    print("\nAll dependencies are up to date.")


if __name__ == "__main__":
  main()
