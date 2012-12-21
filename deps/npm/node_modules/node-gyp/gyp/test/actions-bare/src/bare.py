#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

f = open(sys.argv[1], 'wb')
f.write('Hello from bare.py\n')
f.close()
