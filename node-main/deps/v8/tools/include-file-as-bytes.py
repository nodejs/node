#!/usr/bin/env python3
# Copyright 2025 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This program takes a file and produces another file that contains its binary
bytes stored as uint32_ts.

The first argument is the source path, the second argument is the destination
path, the third is the name of the produced static.

This program could be extended in the future to make uint8_t/etc arrays.

This is based on ICU4C's make_data_cpp.py, but it doesn't require the file to
have any special headers.
"""

import mmap
import os
import struct
import sys

HEADER = """#include <cstdint>
#include <cstddef>

extern "C" uint32_t {0}[] = {{"""

FOOTER = """
}};

extern "C" size_t {0}_len = {1};
"""


def writeFile(src_path, dst_path, name):
  assert os.path.exists(src_path)

  with open(dst_path, 'w') as dst:
    dst.write(HEADER.format(name))
    with open(src_path, 'rb') as src:
      # We wish to read unsigned ints
      s = struct.Struct('@I')
      assert s.size == 4
      mm = mmap.mmap(src.fileno(), 0, access=mmap.ACCESS_READ)
      assert len(mm) % s.size == 0

      # We want to have 16 values per line
      for offset in range(0, len(mm), s.size):
        u32 = s.unpack_from(mm, offset)[0]
        if offset % 16 == 0:
          dst.write("\n   ")
        dst.write(" 0x{:08x},".format(u32))

      dst.write(FOOTER.format(name, int(len(mm) / s.size)))
      mm.close()


if __name__ == '__main__':
  writeFile(sys.argv[1], sys.argv[2], sys.argv[3])
