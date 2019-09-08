# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function

import json
import os
import platform
import subprocess
import sys
import time

from . import base


# Base dir of the build products for Release and Debug.
OUT_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', 'out'))


def print_failure_header(test):
  if test.output_proc.negative:
    negative_marker = '[negative] '
  else:
    negative_marker = ''
  print("=== %(label)s %(negative)s===" % {
    'label': test,
    'negative': negative_marker,
  })


class ResultsTracker(base.TestProcObserver):
  """Tracks number of results and stops to run tests if max_failures reached."""
  def __init__(self, max_failures):
    super(ResultsTracker, self).__init__()
    self._requirement = base.DROP_OUTPUT

    self.failed = 0
    self.remaining = 0
    self.total = 0
    self.max_failures = max_failures

  def _on_next_test(self, test):
    self.total += 1
    self.remaining += 1

  def _on_result_for(self, test, result):
    self.remaining -= 1
    if result.has_unexpected_output:
      self.failed += 1
      if self.max_failures and self.failed >= self.max_failures:
        print('>>> Too many failures, exiting...')
        self.stop()


class ProgressIndicator(base.TestProcObserver):
  def finished(self):
    pass


class SimpleProgressIndicator(ProgressIndicator):
  def __init__(self):
    super(SimpleProgressIndicator, self).__init__()
    self._requirement = base.DROP_PASS_OUTPUT

    self._failed = []

  def _on_result_for(self, test, result):
    # TODO(majeski): Support for dummy/grouped results
    if result.has_unexpected_output:
      self._failed.append((test, result))

  def finished(self):
    crashed = 0
    print()
    for test, result in self._failed:
      print_failure_header(test)
      if result.output.stderr:
        print("--- stderr ---")
        print(result.output.stderr.strip())
      if result.output.stdout:
        print("--- stdout ---")
        print(result.output.stdout.strip())
      print("Command: %s" % result.cmd.to_string())
      if result.output.HasCrashed():
        print("exit code: %d" % result.output.exit_code)
        print("--- CRASHED ---")
        crashed += 1
      if result.output.HasTimedOut():
        print("--- TIMEOUT ---")
    if len(self._failed) == 0:
      print("===")
      print("=== All tests succeeded")
      print("===")
    else:
      print()
      print("===")
      print("=== %i tests failed" % len(self._failed))
      if crashed > 0:
        print("=== %i tests CRASHED" % crashed)
      print("===")


class VerboseProgressIndicator(SimpleProgressIndicator):
  def __init__(self):
    super(VerboseProgressIndicator, self).__init__()
    self._last_printed_time = time.time()

  def _print(self, text):
    print(text)
    sys.stdout.flush()
    self._last_printed_time = time.time()

  def _on_result_for(self, test, result):
    super(VerboseProgressIndicator, self)._on_result_for(test, result)
    # TODO(majeski): Support for dummy/grouped results
    if result.has_unexpected_output:
      if result.output.HasCrashed():
        outcome = 'CRASH'
      else:
        outcome = 'FAIL'
    else:
      outcome = 'pass'

    self._print('Done running %s %s: %s' % (
      test, test.variant or 'default', outcome))

  # TODO(machenbach): Remove this platform specific hack and implement a proper
  # feedback channel from the workers, providing which tests are currently run.
  def _print_processes_linux(self):
    if platform.system() == 'Linux':
      try:
        cmd = 'ps -aux | grep "%s"' % OUT_DIR
        output = subprocess.check_output(cmd, shell=True)
        self._print('List of processes:')
        for line in (output or '').splitlines():
          # Show command with pid, but other process info cut off.
          self._print('pid: %s cmd: %s' %
                      (line.split()[1], line[line.index(OUT_DIR):]))
      except:
        pass

  def _on_heartbeat(self):
    if time.time() - self._last_printed_time > 30:
      # Print something every 30 seconds to not get killed by an output
      # timeout.
      self._print('Still working...')
      self._print_processes_linux()

  def _on_event(self, event):
    self._print(event)
    self._print_processes_linux()


class DotsProgressIndicator(SimpleProgressIndicator):
  def __init__(self):
    super(DotsProgressIndicator, self).__init__()
    self._count = 0

  def _on_result_for(self, test, result):
    super(DotsProgressIndicator, self)._on_result_for(test, result)
    # TODO(majeski): Support for dummy/grouped results
    self._count += 1
    if self._count > 1 and self._count % 50 == 1:
      sys.stdout.write('\n')
    if result.has_unexpected_output:
      if result.output.HasCrashed():
        sys.stdout.write('C')
        sys.stdout.flush()
      elif result.output.HasTimedOut():
        sys.stdout.write('T')
        sys.stdout.flush()
      else:
        sys.stdout.write('F')
        sys.stdout.flush()
    else:
      sys.stdout.write('.')
      sys.stdout.flush()


class CompactProgressIndicator(ProgressIndicator):
  def __init__(self, templates):
    super(CompactProgressIndicator, self).__init__()
    self._requirement = base.DROP_PASS_OUTPUT

    self._templates = templates
    self._last_status_length = 0
    self._start_time = time.time()

    self._passed = 0
    self._failed = 0

  def set_test_count(self, test_count):
    self._total = test_count

  def _on_result_for(self, test, result):
    # TODO(majeski): Support for dummy/grouped results
    if result.has_unexpected_output:
      self._failed += 1
    else:
      self._passed += 1

    self._print_progress(str(test))
    if result.has_unexpected_output:
      output = result.output
      stdout = output.stdout.strip()
      stderr = output.stderr.strip()

      self._clear_line(self._last_status_length)
      print_failure_header(test)
      if len(stdout):
        print(self._templates['stdout'] % stdout)
      if len(stderr):
        print(self._templates['stderr'] % stderr)
      print("Command: %s" % result.cmd.to_string(relative=True))
      if output.HasCrashed():
        print("exit code: %d" % output.exit_code)
        print("--- CRASHED ---")
      if output.HasTimedOut():
        print("--- TIMEOUT ---")

  def finished(self):
    self._print_progress('Done')
    print()

  def _print_progress(self, name):
    self._clear_line(self._last_status_length)
    elapsed = time.time() - self._start_time
    if self._total:
      progress = (self._passed + self._failed) * 100 // self._total
    else:
      progress = 0
    status = self._templates['status_line'] % {
      'passed': self._passed,
      'progress': progress,
      'failed': self._failed,
      'test': name,
      'mins': int(elapsed) / 60,
      'secs': int(elapsed) % 60
    }
    status = self._truncate(status, 78)
    self._last_status_length = len(status)
    print(status, end='')
    sys.stdout.flush()

  def _truncate(self, string, length):
    if length and len(string) > (length - 3):
      return string[:(length - 3)] + "..."
    else:
      return string

  def _clear_line(self, last_length):
    raise NotImplementedError()


class ColorProgressIndicator(CompactProgressIndicator):
  def __init__(self):
    templates = {
      'status_line': ("[%(mins)02i:%(secs)02i|"
                      "\033[34m%%%(progress) 4d\033[0m|"
                      "\033[32m+%(passed) 4d\033[0m|"
                      "\033[31m-%(failed) 4d\033[0m]: %(test)s"),
      'stdout': "\033[1m%s\033[0m",
      'stderr': "\033[31m%s\033[0m",
    }
    super(ColorProgressIndicator, self).__init__(templates)

  def _clear_line(self, last_length):
    print("\033[1K\r", end='')


class MonochromeProgressIndicator(CompactProgressIndicator):
  def __init__(self):
    templates = {
      'status_line': ("[%(mins)02i:%(secs)02i|%%%(progress) 4d|"
                      "+%(passed) 4d|-%(failed) 4d]: %(test)s"),
      'stdout': '%s',
      'stderr': '%s',
    }
    super(MonochromeProgressIndicator, self).__init__(templates)

  def _clear_line(self, last_length):
    print(("\r" + (" " * last_length) + "\r"), end='')


class JsonTestProgressIndicator(ProgressIndicator):
  def __init__(self, framework_name, json_test_results, arch, mode):
    super(JsonTestProgressIndicator, self).__init__()
    # We want to drop stdout/err for all passed tests on the first try, but we
    # need to get outputs for all runs after the first one. To accommodate that,
    # reruns are set to keep the result no matter what requirement says, i.e.
    # keep_output set to True in the RerunProc.
    self._requirement = base.DROP_PASS_STDOUT

    self.framework_name = framework_name
    self.json_test_results = json_test_results
    self.arch = arch
    self.mode = mode
    self.results = []
    self.tests = []

  def _on_result_for(self, test, result):
    if result.is_rerun:
      self.process_results(test, result.results)
    else:
      self.process_results(test, [result])

  def process_results(self, test, results):
    for run, result in enumerate(results):
      # TODO(majeski): Support for dummy/grouped results
      output = result.output
      # Buffer all tests for sorting the durations in the end.
      # TODO(machenbach): Running average + buffer only slowest 20 tests.
      self.tests.append((test, output.duration, result.cmd))

      # Omit tests that run as expected on the first try.
      # Everything that happens after the first run is included in the output
      # even if it flakily passes.
      if not result.has_unexpected_output and run == 0:
        continue

      self.results.append({
        "name": str(test),
        "flags": result.cmd.args,
        "command": result.cmd.to_string(relative=True),
        "run": run + 1,
        "stdout": output.stdout,
        "stderr": output.stderr,
        "exit_code": output.exit_code,
        "result": test.output_proc.get_outcome(output),
        "expected": test.expected_outcomes,
        "duration": output.duration,
        "random_seed": test.random_seed,
        "target_name": test.get_shell(),
        "variant": test.variant,
        "variant_flags": test.variant_flags,
        "framework_name": self.framework_name,
      })

  def finished(self):
    complete_results = []
    if os.path.exists(self.json_test_results):
      with open(self.json_test_results, "r") as f:
        # Buildbot might start out with an empty file.
        complete_results = json.loads(f.read() or "[]")

    duration_mean = None
    if self.tests:
      # Get duration mean.
      duration_mean = (
          sum(duration for (_, duration, cmd) in self.tests) /
          float(len(self.tests)))

    # Sort tests by duration.
    self.tests.sort(key=lambda __duration_cmd: __duration_cmd[1], reverse=True)
    slowest_tests = [
      {
        "name": str(test),
        "flags": cmd.args,
        "command": cmd.to_string(relative=True),
        "duration": duration,
        "marked_slow": test.is_slow,
      } for (test, duration, cmd) in self.tests[:20]
    ]

    complete_results.append({
      "arch": self.arch,
      "mode": self.mode,
      "results": self.results,
      "slowest_tests": slowest_tests,
      "duration_mean": duration_mean,
      "test_total": len(self.tests),
    })

    with open(self.json_test_results, "w") as f:
      f.write(json.dumps(complete_results))
