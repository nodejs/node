#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import os
import sys

DECLARE_FILE = "src/assembler.h"
REGISTER_FILE = "src/external-reference-table.cc"
DECLARE_RE = re.compile("\s*static ExternalReference ([^(]+)\(")
REGISTER_RE = re.compile("\s*Add\(ExternalReference::([^(]+)\(")

WORKSPACE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))

# Ignore those.
BLACKLISTED = [
  "fixed_typed_array_base_data_offset",
  "page_flags",
  "math_exp_constants",
  "math_exp_log_table",
  "ForDeoptEntry",
]

def Find(filename, re):
  references = []
  with open(filename, "r") as f:
    for line in f:
      match = re.match(line)
      if match:
        references.append(match.group(1))
  return references

def Main():
  declarations = Find(DECLARE_FILE, DECLARE_RE)
  registrations = Find(REGISTER_FILE, REGISTER_RE)
  difference = list(set(declarations) - set(registrations) - set(BLACKLISTED))
  for reference in difference:
    print("Declared but not registered: ExternalReference::%s" % reference)
  return len(difference) > 0

if __name__ == "__main__":
  sys.exit(Main())
