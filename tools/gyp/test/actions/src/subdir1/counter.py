#!/usr/bin/env python

# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import time

output = sys.argv[1]
persistoutput = "%s.persist" % sys.argv[1]

count = 0
try:
  count = open(persistoutput, 'r').read()
except:
  pass
count = int(count) + 1

if len(sys.argv) > 2:
  max_count = int(sys.argv[2])
  if count > max_count:
    count = max_count

oldcount = 0
try:
  oldcount = open(output, 'r').read()
except:
  pass

# Save the count in a file that is undeclared, and thus hidden, to gyp. We need
# to do this because, prior to running commands, some build systems deletes
# any declared outputs, so we would lose our count if we just wrote to the
# given output file.
open(persistoutput, 'w').write('%d' % (count))

# Only write the given output file if the count has changed.
if int(oldcount) != count:
  open(output, 'w').write('%d' % (count))
  # Sleep so the next run changes the file time sufficiently to make the build
  # detect the file as changed.
  time.sleep(1)

sys.exit(0)
