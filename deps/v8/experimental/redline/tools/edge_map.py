#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Maps the coverage edges of a d8 build to source lines.

d8 must be built with v8_redline=true,
sanitizer_coverage_flags="inline-8bit-counters,pc-table" and symbol_level=1 (or
more). The script runs d8 once with REDLINE_PC_TABLE set
(experimental/redline/d8/cov.cc writes the PC of each edge), then gives the
addresses to llvm-symbolizer. The output is a gzipped JSON lines file: a header
line, then one line per edge:

  {"e": EDGE, "f": [[FILE, LINE, FUNCTION], ...]}

EDGE is the edge index of Redline (the counter index plus 1). The frames are
innermost first. FILE is relative to the V8 root. With symbol_level=1, there is
one frame: the file and line of the innermost (maybe inlined) code, and the
function that contains the address.

The output is cached next to d8 and made again only for a new build id.

Usage:
  edge_map.py --d8=out/rl_counters/d8 [--out=FILE] [--jobs=N] [--force]
"""

import argparse
import concurrent.futures
import gzip
import json
import os
import subprocess
import sys
import tempfile

V8_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
DEFAULT_SYMBOLIZER = os.path.join(V8_ROOT, 'third_party', 'llvm-build',
                                  'Release+Asserts', 'bin', 'llvm-symbolizer')
CHUNK = 50000


def default_output(d8):
  return os.path.join(
      os.path.dirname(os.path.abspath(d8)), 'redline', 'edges.jsonl.gz')


def read_header(path):
  """Returns the header object of an edge map, or None."""
  try:
    with gzip.open(path, 'rt') as f:
      return json.loads(f.readline())
  except (OSError, ValueError):
    return None


def dump_pc_table(d8):
  """Runs d8 once and returns (build_id, [address, ...])."""
  with tempfile.TemporaryDirectory() as tmp:
    table = os.path.join(tmp, 'pcs.txt')
    env = dict(os.environ, REDLINE_PC_TABLE=table)
    env.pop('SHM_ID', None)
    subprocess.run([d8, '-e', ''],
                   env=env,
                   stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL,
                   check=False)
    if not os.path.exists(table):
      sys.exit('%s did not write a PC table. Build it with '
               'sanitizer_coverage_flags="inline-8bit-counters,pc-table".' % d8)
    with open(table) as f:
      header = f.readline().split()
      fields = dict(item.split('=', 1) for item in header if '=' in item)
      addresses = [line.split()[0] for line in f if line.strip()]
  if int(fields.get('count', '-1')) != len(addresses):
    sys.exit('The PC table of %s is incomplete' % d8)
  return fields.get('build_id', ''), addresses


def normalize(path):
  """Makes a path of the line tables relative to the V8 root."""
  path = path.replace('\\', '/')
  while path.startswith('./'):
    path = path[2:]
  while path.startswith('../'):
    path = path[3:]
  return path


def symbolize(symbolizer, d8, addresses):
  """Returns one list of frames per address."""
  process = subprocess.run([
      symbolizer, '--obj=' + d8, '--inlining', '--output-style=JSON',
      '--functions=linkage', '--demangle'
  ],
                           input='\n'.join(addresses) + '\n',
                           capture_output=True,
                           text=True,
                           check=True)
  result = []
  for line in process.stdout.splitlines():
    line = line.strip()
    if not line:
      continue
    entry = json.loads(line)
    frames = []
    for symbol in entry.get('Symbol', []):
      file_name = normalize(symbol.get('FileName', ''))
      if not file_name:
        continue
      frames.append(
          [file_name,
           symbol.get('Line', 0),
           symbol.get('FunctionName', '')])
    result.append(frames)
  if len(result) != len(addresses):
    sys.exit('llvm-symbolizer returned %d results for %d addresses' %
             (len(result), len(addresses)))
  return result


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
  parser.add_argument('--d8', required=True)
  parser.add_argument('--out')
  parser.add_argument('--symbolizer', default=DEFAULT_SYMBOLIZER)
  parser.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
  parser.add_argument('--force', action='store_true')
  args = parser.parse_args(argv)

  d8 = os.path.abspath(args.d8)
  out = args.out or default_output(d8)
  build_id, addresses = dump_pc_table(d8)
  header = read_header(out)
  if (not args.force and header is not None and
      header.get('build_id') == build_id and build_id):
    print('%s is up to date (build id %s)' % (out, build_id))
    return 0

  chunks = [addresses[i:i + CHUNK] for i in range(0, len(addresses), CHUNK)]
  print('symbolizing %d edges in %d chunks with %d jobs...' %
        (len(addresses), len(chunks), args.jobs))
  with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
    results = list(
        pool.map(lambda chunk: symbolize(args.symbolizer, d8, chunk), chunks))

  os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
  tmp = out + '.tmp'
  with gzip.open(tmp, 'wt') as f:
    f.write(
        json.dumps({
            'redline_edge_map': 1,
            'build_id': build_id,
            'edges': len(addresses),
            'd8': d8,
        }) + '\n')
    edge = 1
    for chunk in results:
      for frames in chunk:
        f.write(json.dumps({'e': edge, 'f': frames}) + '\n')
        edge += 1
  os.replace(tmp, out)
  print('wrote %s (%d edges, build id %s)' % (out, len(addresses), build_id))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
