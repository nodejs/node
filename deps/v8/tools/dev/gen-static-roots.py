#!/usr/bin/env python3

# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import subprocess
import argparse
import os
import filecmp
import tempfile
import shutil
import platform
import re
import sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

# List of all supported build configurations for static roots

STATIC_ROOT_CONFIGURATIONS = {
    "intl-wasm": {
        "gn_args": {
            "v8_enable_static_roots": "false",
            "v8_enable_static_roots_generation": "true",
            "v8_enable_pointer_compression": "true",
            "v8_enable_pointer_compression_shared_cage": "true",
            "v8_enable_webassembly": "true",
            "v8_enable_i18n_support": "true",
            "dcheck_always_on": "false"
        }
    },
    "intl-nowasm": {
        "gn_args": {
            "v8_enable_static_roots": "false",
            "v8_enable_static_roots_generation": "true",
            "v8_enable_pointer_compression": "true",
            "v8_enable_pointer_compression_shared_cage": "true",
            "v8_enable_webassembly": "false",
            "v8_enable_i18n_support": "true",
            "dcheck_always_on": "false"
        }
    },
    "nointl-wasm": {
        "gn_args": {
            "v8_enable_static_roots": "false",
            "v8_enable_static_roots_generation": "true",
            "v8_enable_pointer_compression": "true",
            "v8_enable_pointer_compression_shared_cage": "true",
            "v8_enable_webassembly": "true",
            "v8_enable_i18n_support": "false",
            "dcheck_always_on": "false"
        }
    },
    "nointl-nowasm": {
        "gn_args": {
            "v8_enable_static_roots": "false",
            "v8_enable_static_roots_generation": "true",
            "v8_enable_pointer_compression": "true",
            "v8_enable_pointer_compression_shared_cage": "true",
            "v8_enable_webassembly": "false",
            "v8_enable_i18n_support": "false",
            "dcheck_always_on": "false"
        }
    },
}

# Parse args

parser = argparse.ArgumentParser(description='Generates static-roots.h.')
parser.add_argument(
    '-c',
    '--configuration',
    choices=STATIC_ROOT_CONFIGURATIONS.keys(),
    action='extend',
    default=None,
    nargs='*',
    help="""Build configuration. Refers to a set of configurations with
identical static-roots.h.""")
parser.add_argument(
    '--out',
    default=Path('out'),
    required=False,
    type=Path,
    help='target build directory')
parser.add_argument(
    '-p',
    '--parallel',
    action=argparse.BooleanOptionalAction,
    default=True,
    help='Run builds in parallel')

args = parser.parse_args()

# Some helpers
def run(cmd, **kwargs):
  print(f"# CMD: {cmd}")
  return subprocess.run(cmd, **kwargs, check=True)


def machine_target():
  raw = platform.machine()
  raw_lower = raw.lower()
  if raw_lower == "x86_64" or raw_lower == "amd64":
    return "x64"
  if raw_lower == "aarch64":
    return "arm64"
  return raw


V8_PATH = Path(__file__).parents[2]
GM = V8_PATH / 'tools' / 'dev' / 'gm.py'


def build_and_check(target):
  print(f"-> Building: {target}")
  # Currently the out dir always needs to be exactly 2 levels deep relative to
  # the project root or siso fails to remote compile (see b/429331398).
  build_prefix = f"_gen-static-roots-{target}"
  build_dir = args.out / f"{build_prefix}-{machine_target()}.release"
  if not build_dir.exists():
    build_dir.mkdir(parents=True, exist_ok=True)
  config = STATIC_ROOT_CONFIGURATIONS[target]

  # Let gm create the default config
  e = dict(os.environ)
  e['V8_GM_OUTDIR'] = f"{args.out}"
  e['V8_GM_BUILD_DIR_PREFIX'] = build_prefix
  run([f"{sys.executable}", f"{GM}", f"{machine_target()}.release.gn_args"],
      env=e)

  # Patch the default config according to our needs
  gn_args_template = config["gn_args"].copy()
  gn_args = (build_dir / "args.gn").open("r").read().splitlines()

  needs_updating = False
  found = 0
  for line in filter(bool, gn_args):
    result = re.search(r"^([^ ]+) = (.+)$", line)
    if not result or len(result.groups()) != 2:
      print(f"Error parsing {build_dir / 'args.gn'} at line '{line}'")
      exit(255)
    if result.group(1) in gn_args_template:
      found += 1
      if result.group(2) != f"{gn_args_template[result.group(1)]}":
        needs_updating = True

  if needs_updating or found < len(gn_args_template):
    print("# Updating gn args")
    with (build_dir / "args.gn").open("w") as f:
      for line in filter(bool, gn_args):
        result = re.search(r"^([^ ]+) = (.+)$", line)
        if result.group(1) in gn_args_template:
          line = f"{result.group(1)} = {gn_args_template[result.group(1)]}"
          gn_args_template.pop(result.group(1))
        f.write(f"{line}\n")
      for extra, val in gn_args_template.items():
        f.write(f"{extra} = {val}\n")

  # Build mksnapshot
  run([f"{sys.executable}", f"{GM}", f"{machine_target()}.release.mksnapshot"],
      env=e)

  # Generate static roots file and check if it changed
  filename = f"static-roots-{target}.h"
  out_file = Path(tempfile.gettempdir()) / filename
  run([f"{build_dir / 'mksnapshot'}", "--static-roots-src", f"{out_file}"])
  target_file = V8_PATH / 'src' / 'roots' / filename
  if not filecmp.cmp(out_file, target_file):
    shutil.move(out_file, target_file)
    return True
  return False


all_configs = (
    args.configuration
    if args.configuration else STATIC_ROOT_CONFIGURATIONS.keys())

if args.parallel:
  with ThreadPoolExecutor() as executor:
    results = executor.map(build_and_check, all_configs)
    changed = any(results)
else:
  changed = False
  for target in all_configs:
    if build_and_check(target):
      changed = True

if changed:
  exit(1)
