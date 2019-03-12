#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

contents = 'Hello from make-subdir-file.py\n'

open(sys.argv[1], 'w').write(contents)
