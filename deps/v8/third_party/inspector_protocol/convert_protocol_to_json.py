#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json
import os.path
import re
import sys

import pdl

def main(argv):
    if len(argv) < 2:
        sys.stderr.write("Usage: %s <protocol.pdl> <protocol.json>\n" % sys.argv[0])
        return 1
    file_name = os.path.normpath(argv[0])
    input_file = open(file_name, "r")
    pdl_string = input_file.read()
    protocol = pdl.loads(pdl_string, file_name)
    input_file.close()
    output_file = open(argv[0].replace('.pdl', '.json'), 'wb')
    json.dump(protocol, output_file, indent=4, separators=(',', ': '))
    output_file.close()

    output_file = open(os.path.normpath(argv[1]), 'wb')
    json.dump(protocol, output_file, indent=4, separators=(',', ': '))
    output_file.close()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
