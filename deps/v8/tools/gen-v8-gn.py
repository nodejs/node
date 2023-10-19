#!/usr/bin/env python3

# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

if (sys.version_info >= (3, 0)):
  from io import StringIO
else:
  from io import BytesIO as StringIO


def parse_args():
  global args
  parser = argparse.ArgumentParser()
  parser.add_argument('-o', '--output', type=str, action='store',
                      help='Location of header file to generate')
  parser.add_argument('-p', '--positive-define', type=str, action='append',
                      help='Externally visibile positive definition')
  parser.add_argument('-n', '--negative-define', type=str, action='append',
                      help='Externally visibile negative definition')
  args = parser.parse_args()

def generate_positive_definition(out, define):
  out.write('''
#ifndef {define}
#define {define} 1
#else
#if {define} != 1
#error "{define} defined but not set to 1"
#endif
#endif  // {define}
'''.format(define=define))

def generate_negative_definition(out, define):
  out.write('''
#ifdef {define}
#error "{define} is defined but is disabled by V8's GN build arguments"
#endif  // {define}
'''.format(define=define))

def generate_header(out):
  out.write('''// AUTOMATICALLY GENERATED. DO NOT EDIT.

// The following definitions were used when V8 itself was built, but also appear
// in the externally-visible header files and so must be included by any
// embedder. This will be done automatically if V8_GN_HEADER is defined.
// Ready-compiled distributions of V8 will need to provide this generated header
// along with the other headers in include.

// This header must be stand-alone because it is used across targets without
// introducing dependencies. It should only be included via v8config.h.
''')
  if args.positive_define:
    for define in args.positive_define:
      generate_positive_definition(out, define)

  if args.negative_define:
    for define in args.negative_define:
      generate_negative_definition(out, define)

def main():
  parse_args()
  header_stream = StringIO("")
  generate_header(header_stream)
  contents = header_stream.getvalue()
  if args.output:
    # Check if the contents has changed before writing so we don't cause build
    # churn.
    old_contents = None
    if os.path.exists(args.output):
      with open(args.output, 'r') as f:
        old_contents = f.read()
    if old_contents != contents:
      with open(args.output, 'w') as f:
        f.write(contents)
  else:
    print(contents)

if __name__ == '__main__':
  main()
