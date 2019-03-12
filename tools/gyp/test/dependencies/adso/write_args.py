#!/usr/bin/env python

# Copyright 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

f = open(sys.argv[1], 'w')
f.write(' '.join(sys.argv[2:]))
f.close()
