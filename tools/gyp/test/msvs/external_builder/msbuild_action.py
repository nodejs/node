# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

with open('msbuild_action.out', 'w') as f:
  f.write(' '.join(sys.argv))

