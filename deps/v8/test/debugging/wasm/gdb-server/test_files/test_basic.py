# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# 0x00 (module
# 0x08   [type]
#        (type $type0 (func (param i32 i32) (result i32)))
# 0x11   [function]
# 0x15   (export "mul" (func $func0))
# 0x1e   [code]
# 0x20   (func $func0 (param $var0 i32) (param $var1 i32) (result i32)
# 0x23     get_local $var0
# 0x25     get_local $var1
# 0x27     i32.mul
#   )
# )
# 0x29   [name]

BREAK_ADDRESS_0 = 0x0023
BREAK_ADDRESS_1 = 0x0025
BREAK_ADDRESS_2 = 0x0027
ARG_0 = 21
ARG_1 = 2
