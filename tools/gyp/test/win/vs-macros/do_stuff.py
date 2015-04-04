# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

input = open(sys.argv[1], "r").read()
open(sys.argv[2], "w").write(input + "Modified.")
