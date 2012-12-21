#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Prints interesting vars
"""

import sys;

out = open(sys.argv[2], 'w')
out.write(sys.argv[1]);
