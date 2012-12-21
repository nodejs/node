#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

assert len(sys.argv) == 4, sys.argv

(in_file, c_file, h_file) = sys.argv[1:]

def write_file(filename, contents):
  open(filename, 'wb').write(contents)

write_file(c_file, open(in_file, 'rb').read())

write_file(h_file, '#define NAME "%s"\n' % in_file)

sys.exit(0)
