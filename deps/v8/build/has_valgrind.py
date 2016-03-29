#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VALGRIND_DIR = os.path.join(BASE_DIR, 'third_party', 'valgrind')
LINUX32_DIR = os.path.join(VALGRIND_DIR, 'linux_x86')
LINUX64_DIR = os.path.join(VALGRIND_DIR, 'linux_x64')


def DoMain(_):
  """Hook to be called from gyp without starting a separate python
  interpreter."""
  return int(os.path.exists(LINUX32_DIR) and os.path.exists(LINUX64_DIR))


if __name__ == '__main__':
  print DoMain([])
