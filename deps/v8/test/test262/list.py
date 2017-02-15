#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tarfile
from itertools import chain

os.chdir(os.path.dirname(os.path.abspath(__file__)))

for root, dirs, files in chain(os.walk("data"), os.walk("harness")):
  dirs[:] = [d for d in dirs if not d.endswith('.git')]
  for name in files:
    # These names are for gyp, which expects slashes on all platforms.
    print('/'.join(root.split(os.sep) + [name]))
