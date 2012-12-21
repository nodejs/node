#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys

contents = open(sys.argv[1], 'r').read()
open(sys.argv[2], 'wb').write(contents)

sys.exit(0)
