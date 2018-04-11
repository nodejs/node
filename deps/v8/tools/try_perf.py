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
  '--linux64_atom': 'v8_linux64_atom_perf_try',
  '--linux64_haswell': 'v8_linux64_haswell_perf_try',
  '--nexus5': 'v8_nexus5_perf_try',
  '--nexus7': 'v8_nexus7_perf_try',
  '--nexus9': 'v8_nexus9_perf_try',
  '--nexus10': 'v8_nexus10_perf_try',
}

# This list will contain builder names that should be triggered on an internal
# swarming bucket instead of internal Buildbot master.
SWARMING_BOTS = [
  'v8_linux64_perf_try',
]

DEFAULT_BOTS = [
  'v8_arm32_perf_try',
  'v8_linux32_perf_try',
  'v8_linux64_haswell_perf_try',
  'v8_nexus10_perf_try',
]

PUBLIC_BENCHMARKS = [
  'arewefastyet',
  'embenchen',
  'emscripten',
  'compile',
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
]

V8_BASE = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

def _trigger_bots(bucket, bots, options):
  cmd = ['git cl try']
  cmd += ['-B', bucket]
  cmd += ['-b %s' % bot for bot in bots]
  if options.revision: cmd += ['-r %s' % options.revision]
  benchmarks = ['"%s"' % benchmark for benchmark in options.benchmarks]
  cmd += ['-p \'testfilter=[%s]\'' % ','.join(benchmarks)]
  if options.extra_flags:
    cmd += ['-p \'extra_flags="%s"\'' % options.extra_flags]
  subprocess.check_call(' '.join(cmd), shell=True, cwd=V8_BASE)

def main():
  parser = argparse.ArgumentParser(description='')
  parser.add_argument('benchmarks', nargs='+', help='The benchmarks to run.')
  parser.add_argument('--extra-flags', default='',
                      help='Extra flags to be passed to the executable.')
  parser.add_argument('-r', '--revision', type=str, default=None,
                      help='Revision (use full hash!) to use for the try job; '
                           'default: the revision will be determined by the '
                           'try server; see its waterfall for more info')
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

  for benchmark in options.benchmarks:
    if benchmark not in PUBLIC_BENCHMARKS:
      print ('%s not found in our benchmark list. The respective trybot might '
            'fail, unless you run something this script isn\'t aware of. '
            'Available public benchmarks: %s' % (benchmark, PUBLIC_BENCHMARKS))
      print 'Proceed anyways? [Y/n] ',
      answer = sys.stdin.readline().strip()
      if answer != "" and answer != "Y" and answer != "y":
        return 1

  assert '"' not in options.extra_flags and '\'' not in options.extra_flags, (
      'Invalid flag specification.')

  # Ensure depot_tools are updated.
  subprocess.check_output(
      'update_depot_tools', shell=True, stderr=subprocess.STDOUT, cwd=V8_BASE)

  buildbot_bots = [bot for bot in options.bots if bot not in SWARMING_BOTS]
  if buildbot_bots:
    _trigger_bots('master.internal.client.v8', buildbot_bots, options)

  swarming_bots = [bot for bot in options.bots if bot in SWARMING_BOTS]
  if swarming_bots:
    _trigger_bots('luci.v8-internal.try', swarming_bots, options)


if __name__ == '__main__':  # pragma: no cover
  sys.exit(main())
