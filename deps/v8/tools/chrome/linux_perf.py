#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
from pathlib import Path
from re import A
import os
import shlex
from signal import SIGQUIT
import subprocess
import signal
import tempfile
import time
import psutil
import multiprocessing

from unittest import result

renderer_cmd_file = Path(__file__).parent / 'linux-perf-renderer-cmd.sh'
assert renderer_cmd_file.is_file()
renderer_cmd_prefix = f"{renderer_cmd_file} --perf-data-prefix=chrome_renderer"

# ==============================================================================

usage = """Usage: %prog $CHROME_BIN [OPTION]... -- [CHROME_OPTION]... [URL]

This script runs linux-perf on all render process with custom V8 logging to get
support to resolve JS function names.

The perf data is written to OUT_DIR separate by renderer process.

See http://v8.dev//linux-perf for more detailed instructions.
"""
parser = optparse.OptionParser(usage=usage)
parser.add_option(
    '--perf-data-dir',
    default=None,
    metavar="OUT_DIR",
    help="Output directory for linux perf profile files")
parser.add_option(
    "--profile-browser-process",
    action="store_true",
    default=False,
    help="Also start linux-perf for the browser process. "
    "By default only renderer processes are sampled. "
    "Outputs 'browser_*.perf.data' in the CDW")
parser.add_option("--timeout", type=int, help="Stop chrome after N seconds")

chrome_options = optparse.OptionGroup(
    parser, "Chrome-forwarded Options",
    "These convenience for a better script experience that are forward directly"
    "to chrome. Any other chrome option can be passed after the '--' arguments"
    "separator.")
chrome_options.add_option("--user-data-dir", dest="user_data_dir", default=None)
chrome_options.add_option("--js-flags", dest="js_flags")
chrome_options.add_option(
    "--renderer-cmd-prefix",
    default=None,
    help=f"Set command prefix, used for each new chrome renderer process."
    "Default: {renderer_cmd_prefix}")
FEATURES_DOC = "See chrome's base/feature_list.h source file for more dertails"
chrome_options.add_option(
    "--enable-features",
    help="Comma-separated list of enabled chrome features. " + FEATURES_DOC)
chrome_options.add_option(
    "--disable-features",
    help="Command-separated list of disabled chrome features. " + FEATURES_DOC)
parser.add_option_group(chrome_options)


# ==============================================================================
def log(*args):
  print("")
  print("=" * 80)
  print(*args)
  print("=" * 80)


# ==============================================================================

(options, args) = parser.parse_args()

if len(args) == 0:
  parser.error("No chrome binary provided")

chrome_bin = Path(args.pop(0))
if not chrome_bin.exists():
  parser.error(f"Chrome '{chrome_bin}' does not exist")

if options.renderer_cmd_prefix is not None:
  if options.perf_data_dir is not None:
    parser.error("Cannot specify --perf-data-dir "
                 "if a custom --renderer-cmd-prefix is provided")
else:
  options.renderer_cmd_prefix = str(renderer_cmd_file)

if options.perf_data_dir is None:
  options.perf_data_dir = Path.cwd()
else:
  options.perf_data_dir = Path(options.perf_data_dir).absolute()

if not options.perf_data_dir.is_dir():
  parser.error(f"--perf-data-dir={options.perf_data_dir} "
               "is not an directory or does not exist.")

if options.timeout and options.timeout < 2:
  parser.error("--timeout should be more than 2 seconds")

# ==============================================================================
old_cwd = Path.cwd()
os.chdir(options.perf_data_dir)

# ==============================================================================
JS_FLAGS_PERF = ("--perf-prof --no-write-protect-code-memory "
                 "--interpreted-frames-native-stack")

with tempfile.TemporaryDirectory(prefix="chrome-") as tmp_dir_path:
  tempdir = Path(tmp_dir_path)
  cmd = [
      str(chrome_bin),
  ]
  if options.user_data_dir is None:
    cmd.append(f"--user-data-dir={tempdir}")
  cmd += [
      "--no-sandbox", "--incognito", "--enable-benchmarking", "--no-first-run",
      "--no-default-browser-check",
      f"--renderer-cmd-prefix={options.renderer_cmd_prefix}",
      f"--js-flags={JS_FLAGS_PERF}"
  ]
  if options.js_flags:
    cmd += [f"--js-flags={options.js_flags}"]
  if options.enable_features:
    cmd += [f"--enable-features={options.enable_features}"]
  if options.disable_features:
    cmd += [f"--disable-features={options.disable_features}"]
  cmd += args
  log("CHROME CMD: ", shlex.join(cmd))

  if options.profile_browser_process:
    perf_data_file = f"{tempdir.name}_browser.perf.data"
    perf_cmd = [
        "perf", "record", "--call-graph=fp", "--freq=max", "--clockid=mono",
        f"--output={perf_data_file}", "--"
    ]
    cmd = perf_cmd + cmd
    log("LINUX PERF CMD: ", shlex.join(cmd))

  if options.timeout is None:
    subprocess.run(cmd)
  else:
    process = subprocess.Popen(cmd)
    time.sleep(options.timeout)
    log(f"QUITING chrome child processes after {options.timeout}s timeout")
    current_process = psutil.Process()
    children = current_process.children(recursive=True)
    for child in children:
      if "chrome" in child.name() or "content_shell" in child.name():
        print(f"  quitting PID={child.pid}")
        child.send_signal(signal.SIGQUIT)
    # Wait for linux-perf to write out files
    time.sleep(1)
    process.send_signal(signal.SIGQUIT)
    process.wait()

# ==============================================================================
log("PARALLEL POST PROCESSING: Injecting JS symbols")


def inject_v8_symbols(perf_dat_file):
  output_file = perf_dat_file.with_suffix(".data.jitted")
  cmd = [
      "perf", "inject", "--jit", f"--input={perf_dat_file}",
      f"--output={output_file}"
  ]
  try:
    subprocess.run(cmd)
    print(f"Processed: {output_file}")
  except:
    print(shlex.join(cmd))
    return None
  return output_file


results = []
with multiprocessing.Pool() as pool:
  results = list(
      pool.imap_unordered(inject_v8_symbols,
                          options.perf_data_dir.glob("*perf.data")))

results = list(filter(lambda x: x is not None, results))
if len(results) == 0:
  print("No perf files were successfully processed"
        " Check for errors or partial results in '{options.perf_data_dir}'")
  exit(1)
log(f"RESULTS in '{options.perf_data_dir}'")
results.sort(key=lambda x: x.stat().st_size)
BYTES_TO_MIB = 1 / 1024 / 1024
for output_file in reversed(results):
  print(
      f"{output_file.name:67}{(output_file.stat().st_size*BYTES_TO_MIB):10.2f}MiB"
  )

log("PPROF EXAMPLE")
path_strings = map(lambda f: str(f.relative_to(old_cwd)), results)
print(f"pprof -flame { ' '.join(path_strings)}")
