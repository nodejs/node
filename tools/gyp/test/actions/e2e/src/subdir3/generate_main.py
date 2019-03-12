#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

contents = """
#include <stdio.h>

int main(void)
{
  printf("Hello from generate_main.py\\n");
  return 0;
}
"""

open(sys.argv[1], 'w').write(contents)

sys.exit(0)
