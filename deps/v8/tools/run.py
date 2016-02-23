#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program wraps an arbitrary command since gn currently can only execute
scripts."""

import subprocess
import sys

sys.exit(subprocess.call(sys.argv[1:]))
