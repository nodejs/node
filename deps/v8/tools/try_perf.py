#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import find_depot_tools
import sys

find_depot_tools.add_depot_tools_to_path()

from git_cl import Changelist

BOTS = [
  'v8_linux32_perf_try',
  'v8_linux64_perf_try',
]

def main(tests):
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

  if not tests:
    print 'Please specify the benchmarks to run as arguments.'
    return 1

  masters = {'internal.client.v8': dict((b, tests) for b in BOTS)}
  cl.RpcServer().trigger_distributed_try_jobs(
        cl.GetIssue(), cl.GetMostRecentPatchset(), cl.GetBranch(),
        False, None, masters)
  return 0

if __name__ == "__main__":  # pragma: no cover
  sys.exit(main(sys.argv[1:]))
