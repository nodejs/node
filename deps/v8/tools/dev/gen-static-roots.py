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
from pathlib import Path

# Detect if we have goma


def _Which(cmd):
  for path in os.environ["PATH"].split(os.pathsep):
    if os.path.exists(os.path.join(path, cmd)):
      return os.path.join(path, cmd)
  return None


def DetectGoma():
  if os.environ.get("GOMA_DIR"):
    return os.environ.get("GOMA_DIR")
  if os.environ.get("GOMADIR"):
    return os.environ.get("GOMADIR")
  # There is a copy of goma in depot_tools, but it might not be in use on
  # this machine.
  goma = _Which("goma_ctl")
  if goma is None:
    return None
  cipd_bin = os.path.join(os.path.dirname(goma), ".cipd_bin")
  if not os.path.exists(cipd_bin):
    return None
  goma_auth = os.path.expanduser("~/.goma_client_oauth2_config")
  if not os.path.exists(goma_auth):
    return None
  return cipd_bin


GOMADIR = DetectGoma()
IS_GOMA_MACHINE = GOMADIR is not None

USE_GOMA = "true" if IS_GOMA_MACHINE else "false"

# List of all supported build configurations for static roots

STATIC_ROOT_CONFIGURATIONS = {
    "ptr-cmpr-wasm-intl": {
        "target":
            "src/roots/static-roots.h",
        "gn_args":
            f"""\
is_debug = false
use_goma = {USE_GOMA}
v8_enable_static_roots = false
v8_enable_static_root_generation = true
v8_enable_pointer_compression = true
v8_enable_shared_ro_heap = true
v8_enable_pointer_compression_shared_cage = true
v8_enable_webassembly = true
v8_enable_i18n_support = true
dcheck_always_on = true
"""
    },
}

# Parse args

parser = argparse.ArgumentParser(description='Generates static-roots.h.')
parser.add_argument(
    '--configuration',
    choices=STATIC_ROOT_CONFIGURATIONS.keys(),
    action='extend',
    default='ptr-cmpr-wasm-intl',
    nargs='*',
    help="""Build configuration. Refers to a set of configurations with
identical static-roots.h. Currently there is only one supported configuration.
Future configurations will need to generate multiple target files.""")
parser.add_argument(
    '--out',
    default=Path('out'),
    required=False,
    type=Path,
    help='target build directory')

args = parser.parse_args()

# Some helpers


def run(cmd, **kwargs):
  print(f"# CMD: {cmd} {kwargs}")
  return subprocess.run(cmd, **kwargs, check=True)


def build(path, gn_args):
  if not path.exists():
    path.mkdir(parents=True, exist_ok=True)
  with (path / "args.gn").open("w") as f:
    f.write(gn_args)
  suffix = ".bat" if platform.system() == "Windows" else ""
  run(["gn" + suffix, "gen", path])
  run(["autoninja" + suffix, "-C", path, "mksnapshot"])
  return path.absolute()


# Generate all requested static root headers

v8_path = Path(__file__).parents[2]

changed = False
for target in [args.configuration]:
  build_dir = args.out / f"gen-static-roots.{target}"
  config = STATIC_ROOT_CONFIGURATIONS[target]
  gn_args = config["gn_args"]
  build_path = build(build_dir, gn_args)
  out_file = Path(tempfile.gettempdir()) / f"static-roots-{target}.h"
  run([build_path / "mksnapshot", "--static-roots-src", out_file])
  target_file = v8_path / config["target"]
  if not filecmp.cmp(out_file, target_file):
    shutil.move(out_file, target_file)
    changed = True

if changed:
  exit(1)
