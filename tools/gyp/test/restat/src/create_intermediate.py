#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

"""
Create argv[1] iff it doesn't already exist.
"""

outfile = sys.argv[1]
if os.path.exists(outfile):
  sys.exit()
open(outfile, "wb").close()
