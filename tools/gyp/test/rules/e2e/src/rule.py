#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

f = open(sys.argv[1] + ".cc", "w")
f.write("""\
#include <stdio.h>

int main() {
  puts("Hello %s");
  return 0;
}
""" % sys.argv[1])
f.close()
