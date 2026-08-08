#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import concurrent.futures
import os
import shutil
import subprocess
import sys
import threading
import time

# Shared state
total_reserved = 0
total_generated = 0
total_crashes = 0
lock = threading.Lock()
stop_fuzzing = False


def run_test(test_info, args, d8_abs):
  global total_generated, total_crashes, stop_fuzzing

  if stop_fuzzing:
    return

  f_name, test_path, flags_path = test_info

  d8_args = [d8_abs, "--fuzzing"]
  if args.extra_flags:
    d8_args.extend(args.extra_flags.split())

  if os.path.exists(flags_path):
    with open(flags_path, "r") as ff:
      flags_content = ff.read().strip()
      if flags_content:
        d8_args.extend(flags_content.split())

  d8_args.append(test_path)

  try:
    # Timeout d8 to avoid hangs
    result = subprocess.run(d8_args, capture_output=True, text=True, timeout=5)

    crash_reason = None
    if result.returncode < 0:
      crash_reason = f"Signal received: {-result.returncode}"
    elif result.returncode > 127:
      crash_reason = f"Likely signal (exit code {result.returncode})"
    elif "FATAL" in result.stderr:
      crash_reason = "FATAL error detected in stderr"
    elif "AddressSanitizer" in result.stderr:
      crash_reason = "AddressSanitizer report detected"

    if crash_reason:
      with lock:
        total_crashes += 1
        crash_id = total_crashes
        if total_crashes >= args.num_crashes and not stop_fuzzing:
          print(f"\n[!] Crash limit reached ({args.num_crashes}). Stopping...")
          stop_fuzzing = True

        crash_name = f"crash_{crash_id}_{f_name}"
        shutil.copy(test_path, os.path.join(args.crash_dir, crash_name))
        if os.path.exists(flags_path):
          shutil.copy(
              flags_path,
              os.path.join(
                  args.crash_dir,
                  f"crash_{crash_id}_{os.path.basename(flags_path)}",
              ),
          )

        print(f"\n[!] CRASH FOUND: {f_name}")
        print(f"    Reason: {crash_reason}")
        print(f"    Exit code: {result.returncode}")
        print(f"    Saved to: {os.path.join(args.crash_dir, crash_name)}")
        # Print a small snippet of stderr for context
        stderr_snippet = result.stderr.strip().splitlines()
        if stderr_snippet:
          print("    Stderr snippet:")
          for line in stderr_snippet[:5]:  # Show first 5 lines
            print(f"      {line}")
          if len(stderr_snippet) > 5:
            print("      ...")

  except subprocess.TimeoutExpired:
    # Timeouts are usually not crashes in this context, just skip
    pass
  except Exception as e:
    with lock:
      print(f"Error running d8: {e}")

  with lock:
    total_generated += 1
    if total_generated >= args.num_tests and not stop_fuzzing:
      print(f"\n[!] Test limit reached ({args.num_tests}). Stopping...")
      stop_fuzzing = True
    if total_generated % 5 == 0:
      sys.stdout.write(f"\rGenerated: {total_generated}/{args.num_tests}, "
                       f"Crashes: {total_crashes}")
      sys.stdout.flush()


def fuzz_batch(
    batch_size,
    args,
    d8_abs,
    fuzzer_abs,
    fuzzer_dir,
    input_abs,
    output_abs,
    app_dir_abs,
):
  global stop_fuzzing

  if stop_fuzzing:
    return

  # Create a unique batch directory to avoid collisions between parallel jobs.
  batch_id = threading.get_ident() + int(time.time() * 1000)
  batch_output_dir = os.path.join(output_abs, f"batch_{batch_id}")
  os.makedirs(batch_output_dir, exist_ok=True)

  try:
    env = os.environ.copy()
    env["APP_NAME"] = "d8"
    env["APP_DIR"] = app_dir_abs

    # Run fuzzer to generate a batch of test cases.
    subprocess.run(
        [
            fuzzer_abs,
            "-i",
            input_abs,
            "-o",
            batch_output_dir,
            "-n",
            str(batch_size),
        ],
        cwd=fuzzer_dir,
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=True,
    )

    # Collect generated files.
    generated_files = [
        f for f in os.listdir(batch_output_dir)
        if f.startswith("fuzz-") and f.endswith(".js")
    ]

    for f in sorted(generated_files):
      if stop_fuzzing:
        break
      test_path = os.path.join(batch_output_dir, f)
      # Extract index to find corresponding flags file.
      idx = f[5:-3]
      flags_path = os.path.join(batch_output_dir, f"flags-{idx}.js")
      run_test((f, test_path, flags_path), args, d8_abs)

  except subprocess.CalledProcessError as e:
    with lock:
      print(f"\n[!] Fuzzer failed with exit code {e.returncode}")
  except Exception as e:
    with lock:
      print(f"\n[!] Unexpected error in fuzz_batch: {e}")
  finally:
    # Cleanup batch dir.
    shutil.rmtree(batch_output_dir)


def main():
  global total_reserved, stop_fuzzing

  # Script is in tools/, js_fuzzer is one level up.
  js_fuzzer_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  # V8 root is four levels up from the script's directory.
  v8_root = os.path.abspath(
      os.path.join(os.path.dirname(__file__), "../../../.."))

  parser = argparse.ArgumentParser(
      description="Simulate Clusterfuzz with JS fuzzer.",
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
  )
  parser.add_argument(
      "--fuzzer",
      default=os.path.join(js_fuzzer_dir, "workdir/fuzzer/ochang_js_fuzzer"),
      help="Path to fuzzer binary",
  )
  parser.add_argument(
      "--input",
      default=os.path.join(js_fuzzer_dir, "workdir/input"),
      help="Input corpus directory",
  )
  parser.add_argument(
      "--app-dir",
      default=os.path.join(js_fuzzer_dir, "workdir/app_dir"),
      help="App directory containing v8_build_config.json",
  )
  parser.add_argument(
      "--d8",
      default=os.path.join(v8_root, "out/x64.release/d8"),
      help="Path to d8 binary",
  )
  parser.add_argument(
      "--output-dir",
      default=os.path.join(js_fuzzer_dir, "workdir/output/generated"),
      help="Directory for generated test cases",
  )
  parser.add_argument(
      "--crash-dir",
      default=os.path.join(js_fuzzer_dir, "workdir/crash"),
      help="Directory to save crashing test cases",
  )
  parser.add_argument(
      "--num-tests",
      type=int,
      default=10000,
      help="Number of test cases to generate",
  )
  parser.add_argument(
      "--num-crashes", type=int, default=10, help="Stop after N crashes")
  parser.add_argument(
      "--batch-size", type=int, default=100, help="Batch size for generation")
  parser.add_argument(
      "--extra-flags", default="", help="Extra flags to always pass to d8")
  parser.add_argument(
      "--jobs",
      type=int,
      default=int(os.cpu_count() / 2),
      help="Number of parallel jobs",
  )

  args = parser.parse_args()

  # Ensure directories exist.
  os.makedirs(args.output_dir, exist_ok=True)
  os.makedirs(args.crash_dir, exist_ok=True)

  fuzzer_abs = os.path.abspath(args.fuzzer)
  fuzzer_dir = os.path.dirname(fuzzer_abs)
  input_abs = os.path.abspath(args.input)
  output_abs = os.path.abspath(args.output_dir)
  app_dir_abs = os.path.abspath(args.app_dir)
  d8_abs = os.path.abspath(args.d8)

  # Validation.
  if not os.path.exists(d8_abs):
    print(f"Error: d8 binary not found at {d8_abs}")
    sys.exit(1)
  if not os.access(d8_abs, os.X_OK):
    print(f"Error: d8 binary at {d8_abs} is not executable")
    sys.exit(1)

  if not os.path.exists(fuzzer_abs):
    print(f"Error: fuzzer binary not found at {fuzzer_abs}")
    sys.exit(1)
  if not os.access(fuzzer_abs, os.X_OK):
    print(f"Error: fuzzer binary at {fuzzer_abs} is not executable")
    sys.exit(1)

  print(f"Starting fuzzer simulation with {args.jobs} jobs...")
  print(f"Fuzzer: {fuzzer_abs}")
  print(f"D8: {d8_abs}")
  print(f"Target: {args.num_tests} tests (batch size: {args.batch_size}, "
        f"jobs: {args.jobs}) or {args.num_crashes} crashes")

  # Initial progress output.
  sys.stdout.write(f"Generated: 0/{args.num_tests}, Crashes: 0")
  sys.stdout.flush()

  try:
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=args.jobs) as executor:
      futures = []
      while total_reserved < args.num_tests and not stop_fuzzing:
        # Keep the queue full but not overflowing to manage memory and resources.
        if len(futures) < args.jobs * 2:
          batch_size = min(args.batch_size, args.num_tests - total_reserved)
          if batch_size <= 0:
            break
          total_reserved += batch_size
          futures.append(
              executor.submit(
                  fuzz_batch,
                  batch_size,
                  args,
                  d8_abs,
                  fuzzer_abs,
                  fuzzer_dir,
                  input_abs,
                  output_abs,
                  app_dir_abs,
              ))

        # Clean up finished futures.
        futures = [f for f in futures if not f.done()]
        time.sleep(0.1)

      # Wait for remaining tasks to finish.
      concurrent.futures.wait(futures)

    print(f"\n\nRun complete.")
    print(f"Total test cases generated: {total_generated}")
    print(f"Total crashes found: {total_crashes}")
    if total_crashes > 0:
      print(f"Crashes saved to {args.crash_dir}/")

  except KeyboardInterrupt:
    print("\nAborted by user.")
    stop_fuzzing = True
  except Exception as e:
    print(f"\nAn error occurred: {e}")


if __name__ == "__main__":
  main()
