#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import argparse

try:
  import json
except ImportError:
  import simplejson as json

import pdl

def main(argv):
  cmdline_parser = argparse.ArgumentParser()
  cmdline_parser.add_argument('filename', nargs='*')
  cmdline_parser.add_argument("--depfile", required=False)
  cmdline_parser.add_argument("--stamp", required=False)
  arg_options = cmdline_parser.parse_args()

  if bool(arg_options.depfile) != bool(arg_options.stamp):
    raise Exception("--depfile requires --stamp and vice versa")

  if len(arg_options.filename) < 1:
    sys.stderr.write(
        "Usage: %s <protocol-1> [<protocol-2> [, <protocol-3>...]] "
        "<output-file>\n" % sys.argv[0])
    return 1

  filenames = arg_options.filename
  deps_filename = arg_options.depfile
  stamp_filename = arg_options.stamp
  domains = []
  source_set = set()
  version = None
  for protocol in filenames[:-1]:
    file_name = os.path.normpath(protocol)
    if not os.path.isfile(file_name):
      sys.stderr.write("Cannot find %s\n" % file_name)
      return 1
    input_file = open(file_name, "r")
    parsed_json = pdl.loads(input_file.read(), file_name, False, source_set)
    domains += parsed_json["domains"]
    version = parsed_json["version"]

  if stamp_filename:
    with open(stamp_filename, "w"):
      pass

  output_file = open(argv[-1], "w")
  json.dump({"version": version, "domains": domains}, output_file,
            indent=4, sort_keys=False, separators=(',', ': '))
  output_file.close()

  if deps_filename:
    assert stamp_filename
    with open(deps_filename, "w") as deps_file:
      deps_file.write("%s: %s\n" %
                      (stamp_filename, " ".join(sorted(source_set))))


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
