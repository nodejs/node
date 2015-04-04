#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

outfile = sys.argv[1]
open(outfile, 'w').write('const char kFoo[] = "%s";' % sys.argv[2])
