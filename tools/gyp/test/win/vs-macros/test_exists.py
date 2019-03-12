# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

if not os.path.exists(sys.argv[1]):
  raise Exception()
open(sys.argv[2], 'w').close()
