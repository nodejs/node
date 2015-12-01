#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

BOTS = {
  '--arm32': 'v8_arm32_perf_try',
  '--linux32': 'v8_linux32_perf_try',
  '--linux64': 'v8_linux64_perf_try',
  '--linux64_haswell': 'v8_linux64_haswell_perf_try',
  '--nexus5': 'v8_nexus5_perf_try',
  '--nexus7': 'v8_nexus7_perf_try',
  '--nexus9': 'v8_nexus9_perf_try',
  '--nexus10': 'v8_nexus10_perf_try',
}

DEFAULT_BOTS = [
  'v8_arm32_perf_try',
  'v8_linux32_perf_try',
  'v8_linux64_haswell_perf_try',
  'v8_nexus10_perf_try',
]

V8_BASE = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

def main():
  parser = argparse.ArgumentParser(description='')
  parser.add_argument('benchmarks', nargs='+', help='The benchmarks to run.')
  parser.add_argument('--extra-flags', default='',
                      help='Extra flags to be passed to the executable.')
  for option in sorted(BOTS):
    parser.add_argument(
        option, dest='bots', action='append_const', const=BOTS[option],
        help='Add %s trybot.' % BOTS[option])
  options = parser.parse_args()
  if not options.bots:
    print 'No trybots specified. Using default %s.' % ','.join(DEFAULT_BOTS)
    options.bots = DEFAULT_BOTS

  if not options.benchmarks:
    print 'Please specify the benchmarks to run as arguments.'
    return 1

  assert '"' not in options.extra_flags and '\'' not in options.extra_flags, (
      'Invalid flag specification.')

  # Ensure depot_tools are updated.
  subprocess.check_output(
      'gclient', shell=True, stderr=subprocess.STDOUT, cwd=V8_BASE)

  cmd = ['git cl try -m internal.client.v8']
  cmd += ['-b %s' % bot for bot in options.bots]
  benchmarks = ['"%s"' % benchmark for benchmark in options.benchmarks]
  cmd += ['-p \'testfilter=[%s]\'' % ','.join(benchmarks)]
  if options.extra_flags:
    cmd += ['-p \'extra_flags="%s"\'' % options.extra_flags]
  subprocess.check_call(' '.join(cmd), shell=True, cwd=V8_BASE)


if __name__ == '__main__':  # pragma: no cover
  sys.exit(main())
