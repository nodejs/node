#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tarfile
from itertools import chain

os.chdir(os.path.dirname(os.path.abspath(__file__)))

for root, dirs, files in chain(os.walk("data"), os.walk("harness")):
  dirs[:] = [d for d in dirs if not d.endswith('.git')]
  for name in files:
    # These names are for gyp, which expects slashes on all platforms.
    pathname = '/'.join(root.split(os.sep) + [name])
    # For gyp, quote the name in case it includes spaces
    if len(sys.argv) > 1 and sys.argv[1] == '--quoted':
      pathname = '"' + pathname + '"'
      # Temporary hack until we upgrade to gn swarming:
      # gyp doesn't handle files containing $ in the name very well, so we just
      # exclude them from the 'sources' list and hope that other changes cause
      # the archive to be rebuilt.
      if '$' in pathname: continue
    print(pathname)
