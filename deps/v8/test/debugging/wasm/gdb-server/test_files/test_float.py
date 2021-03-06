# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# 0x00 (module
# 0x08   [type]
#        (type $type0 (func (param f32 f32) (result f32)))
# 0x11   [function]
# 0x15   (export "mul" (func $func0))
# 0x1e   [code]
# 0x20   (func $func0 (param $var0 f32) (param $var1 f32) (result f32)
# 0x23     get_local $var0
# 0x25     get_local $var1
# 0x27     f32.mul
#   )
# )
# 0x29   [name]

ARG_0 = 12.0
ARG_1 = 3.5
FUNC_START_ADDR = 0x23
