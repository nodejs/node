# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import json
import re
import argparse

sys.path.append(os.environ['PERF_EXEC_PATH'] + \
  '/scripts/python/Perf-Trace-Util/lib/Perf/Trace')

from perf_trace_context import *
from Core import *

def trace_begin():
  json_obj['eventCounts'] = {}
  prog = re.compile(r'0x[0-9a-fA-F]+')
  for phase in reversed(json_obj['phases']):
    if phase['name'] == "disassembly":
      for line in phase['data'].splitlines():
        result = re.match(prog, line)
        if result:
          known_addrs.add(result.group(0))

def trace_end():
  print json.dumps(json_obj)

def process_event(param_dict):
  addr = "0x%x" % int(param_dict['sample']['ip'])

  # Only count samples that belong to the function
  if addr not in known_addrs:
    return

  ev_name = param_dict['ev_name']
  if ev_name not in json_obj['eventCounts']:
    json_obj['eventCounts'][ev_name] = {}
  if addr not in json_obj['eventCounts'][ev_name]:
    json_obj['eventCounts'][ev_name][addr] = 0
  json_obj['eventCounts'][ev_name][addr] += 1

if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Perf script to merge profiling data with turbofan compiler "
                  "traces.")
  parser.add_argument("file_name", metavar="JSON File",
      help="turbo trace json file.")

  args = parser.parse_args()

  with open(args.file_name, 'r') as json_file:
    json_obj = json.load(json_file)

  known_addrs = set()
