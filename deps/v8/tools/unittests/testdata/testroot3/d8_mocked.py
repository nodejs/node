# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Dummy d8 replacement. Just passes all test.
"""

# for py2/py3 compatibility
from __future__ import print_function

import sys

args = ' '.join(sys.argv[1:])
print(args)
sys.exit(0)
