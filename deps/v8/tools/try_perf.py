#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import find_depot_tools
import sys

find_depot_tools.add_depot_tools_to_path()

from git_cl import Changelist

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
  'v8_linux32_perf_try',
  'v8_linux64_haswell_perf_try',
]

def main():
  parser = argparse.ArgumentParser(description='')
  parser.add_argument("benchmarks", nargs="+", help="The benchmarks to run.")
  for option in sorted(BOTS):
    parser.add_argument(
        option, dest='bots', action='append_const', const=BOTS[option],
        help='Add %s trybot.' % BOTS[option])
  options = parser.parse_args()
  if not options.bots:
    print 'No trybots specified. Using default %s.' % ','.join(DEFAULT_BOTS)
    options.bots = DEFAULT_BOTS

  cl = Changelist()
  if not cl.GetIssue():
    print 'Need to upload first'
    return 1

  props = cl.GetIssueProperties()
  if props.get('closed'):
    print 'Cannot send tryjobs for a closed CL'
    return 1

  if props.get('private'):
    print 'Cannot use trybots with private issue'
    return 1

  if not options.benchmarks:
    print 'Please specify the benchmarks to run as arguments.'
    return 1

  masters = {
    'internal.client.v8': dict((b, options.benchmarks) for b in options.bots),
  }
  cl.RpcServer().trigger_distributed_try_jobs(
      cl.GetIssue(), cl.GetMostRecentPatchset(), cl.GetBranch(),
      False, None, masters)
  return 0

if __name__ == "__main__":  # pragma: no cover
  sys.exit(main())
