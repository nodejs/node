#!/usr/bin/env python3
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import os
import subprocess
import sys

BOTS = {
  '--linux32': 'v8_linux32_perf_try',
  '--linux64': 'v8_linux64_perf_try',
  '--nexus5': 'v8_nexus5_perf_try',
  '--pixel2': 'v8_pixel2_perf_try',
}

DEFAULT_BOTS = [
  'v8_linux32_perf_try',
  'v8_linux64_perf_try',
]

PUBLIC_BENCHMARKS = [
  'arewefastyet',
  'ares6',
  'blazor',
  'compile',
  'embenchen',
  'emscripten',
  'jetstream',
  'jsbench',
  'jstests',
  'kraken_orig',
  'massive',
  'memory',
  'octane',
  'octane-noopt',
  'octane-pr',
  'octane-tf',
  'octane-tf-pr',
  'sunspider',
  'unity',
  'wasm',
  'web-tooling-benchmark',
]

V8_BASE = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

def main():
  parser = argparse.ArgumentParser(description='')
  parser.add_argument('benchmarks', nargs='+', help='The benchmarks to run.')
  parser.add_argument('--extra-flags', default='',
                      help='Extra flags to be passed to the executable.')
  parser.add_argument('-r', '--revision', type=str, default=None,
                      help='Revision (use full hash!) to use for the try job; '
                           'default: the revision will be determined by the '
                           'try server; see its waterfall for more info')
  parser.add_argument('-v', '--verbose', action='store_true',
                      help='Print debug information')
  parser.add_argument('-c', '--confidence-level', type=float,
                      help='Repeatedly runs each benchmark until specified '
                      'confidence level is reached. The value is interpreted '
                      'as the number of standard deviations from the mean that '
                      'all values must lie within. Typical values are 1, 2 and '
                      '3 and correspond to 68%%, 95%% and 99.7%% probability '
                      'that the measured value is within 0.1%% of the true '
                      'value. Larger values result in more retries and thus '
                      'longer runtime, but also provide more reliable results.')
  for option in sorted(BOTS):
    parser.add_argument(
        option, dest='bots', action='append_const', const=BOTS[option],
        help='Add %s trybot.' % BOTS[option])
  options = parser.parse_args()
  if not options.bots:
    print('No trybots specified. Using default %s.' % ','.join(DEFAULT_BOTS))
    options.bots = DEFAULT_BOTS

  if not options.benchmarks:
    print('Please specify the benchmarks to run as arguments.')
    return 1

  for benchmark in options.benchmarks:
    if benchmark not in PUBLIC_BENCHMARKS:
      print ('%s not found in our benchmark list. The respective trybot might '
            'fail, unless you run something this script isn\'t aware of. '
            'Available public benchmarks: %s' % (benchmark, PUBLIC_BENCHMARKS))
      print('Proceed anyways? [Y/n] ', end=' ')
      answer = sys.stdin.readline().strip()
      if answer != "" and answer != "Y" and answer != "y":
        return 1

  assert '"' not in options.extra_flags and '\'' not in options.extra_flags, (
      'Invalid flag specification.')

  # Ensure depot_tools are updated.
  subprocess.check_output(
      'update_depot_tools', shell=True, stderr=subprocess.STDOUT, cwd=V8_BASE)

  cmd = ['git cl try', '-B', 'luci.v8-internal.try']
  cmd += ['-b %s' % bot for bot in options.bots]
  if options.revision:
    cmd.append('-r %s' % options.revision)
  benchmarks = ['"%s"' % benchmark for benchmark in options.benchmarks]
  cmd.append('-p \'testfilter=[%s]\'' % ','.join(benchmarks))
  if options.extra_flags:
    cmd.append('-p \'extra_flags="%s"\'' % options.extra_flags)
  if options.confidence_level:
    cmd.append('-p confidence_level=%f' % options.confidence_level)
  if options.verbose:
    cmd.append('-vv')
    print('Running %s' % ' '.join(cmd))
  subprocess.check_call(' '.join(cmd), shell=True, cwd=V8_BASE)

if __name__ == '__main__':  # pragma: no cover
  sys.exit(main())
