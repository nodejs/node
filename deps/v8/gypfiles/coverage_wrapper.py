#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# CC/CXX wrapper script that excludes certain file patterns from coverage
# instrumentation.

import re
import subprocess
import sys

exclusions = [
  'buildtools',
  'src/third_party',
  'third_party',
  'test',
  'testing',
]

def remove_if_exists(string_list, item):
  if item in string_list:
    string_list.remove(item)

args = sys.argv[1:]
text = ' '.join(sys.argv[2:])
for exclusion in exclusions:
  if re.search(r'\-o obj/%s[^ ]*\.o' % exclusion, text):
    remove_if_exists(args, '-fprofile-arcs')
    remove_if_exists(args, '-ftest-coverage')
    remove_if_exists(args, '-fsanitize-coverage=func')
    remove_if_exists(args, '-fsanitize-coverage=bb')
    remove_if_exists(args, '-fsanitize-coverage=edge')
    remove_if_exists(args, '-fsanitize-coverage=trace-pc-guard')
    remove_if_exists(args, '-fsanitize-coverage=bb,trace-pc-guard')
    break

sys.exit(subprocess.check_call(args))
