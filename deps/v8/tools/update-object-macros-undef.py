#!/usr/bin/env python3

# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# vim:fenc=utf-8:shiftwidth=2:tabstop=2:softtabstop=2:extandtab

"""
Generate object-macros-undef.h from object-macros.h.
"""

import os.path
import re
import sys

INPUT = 'src/objects/object-macros.h'
OUTPUT = 'src/objects/object-macros-undef.h'
HEADER = """// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generate this file using the {} script.

// PRESUBMIT_INTENTIONALLY_MISSING_INCLUDE_GUARD

""".format(os.path.basename(__file__))


def main():
  if not os.path.isfile(INPUT):
    sys.exit("Input file {} does not exist; run this script in a v8 checkout."
             .format(INPUT))
  if not os.path.isfile(OUTPUT):
    sys.exit("Output file {} does not exist; run this script in a v8 checkout."
             .format(OUTPUT))
  regexp = re.compile('^#define (\w+)')
  seen = set()
  with open(INPUT, 'r') as infile, open(OUTPUT, 'w') as outfile:
    outfile.write(HEADER)
    for line in infile:
      match = regexp.match(line)
      if match and match.group(1) not in seen:
        seen.add(match.group(1))
        outfile.write('#undef {}\n'.format(match.group(1)))

if __name__ == "__main__":
  main()
