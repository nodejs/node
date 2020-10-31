#!/usr/bin/env python
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
import subprocess
import sys

ROOT_DIR = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'encode_or_decode',
      choices=['encode', 'decode'],
      help='encode into binary format or decode to text.')
  parser.add_argument(
      '--proto_name',
      default='TraceConfig',
      help='name of proto to encode/decode (default: TraceConfig).')
  parser.add_argument(
      '--protoc', default='protoc', help='Path to the protoc executable')
  parser.add_argument(
      '--input',
      default='-',
      help='input file, or "-" for stdin (default: "-")')
  parser.add_argument(
      '--output',
      default='-',
      help='output file, or "-" for stdout (default: "-")')
  args = parser.parse_args()

  cmd = [
      args.protoc,
      '--%s=perfetto.protos.%s' % (args.encode_or_decode, args.proto_name),
      '--proto_path=%s' % ROOT_DIR,
      os.path.join(ROOT_DIR, 'protos/perfetto/config/trace_config.proto'),
      os.path.join(ROOT_DIR, 'protos/perfetto/trace/trace.proto'),
  ]
  in_file = sys.stdin if args.input == '-' else open(args.input, 'rb')
  out_file = sys.stdout if args.output == '-' else open(args.output, 'wb')
  subprocess.check_call(cmd, stdin=in_file, stdout=out_file, stderr=sys.stderr)
  return 0


if __name__ == '__main__':
  sys.exit(main())
