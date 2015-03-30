#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

f = open(sys.argv[1], 'wb')
f.write('int main() {\n')
f.write('  return 0;\n')
f.write('}\n')
f.close()
