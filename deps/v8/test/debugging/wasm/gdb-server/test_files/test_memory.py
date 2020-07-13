# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# 0x00 (module
# 0x08   [type]
# 0x0a   (type $type0 (func (param i32)))
# 0x0f   (type $type1 (func (param i32)))
# 0x13   [function]
# 0x18   [memory]
#        (memory (;0;) 32 128)
# 0x1f   [global]
# 0x27   (global $global0 i32 (i32.const 0))
# 0x29   (export "g_n" (global $global0))
# 0x30   (export "mem" (memory 0))
# 0x36   (export "main" (func $func1))
# 0x3d   [code]
# 0x3f   (func $func0 (param $var0 i32)
# 0x42     nop
# 0x43     nop
#   )
# 0x45   (func $func1 (param $var0 i32)
# 0x47     loop $label0
# 0x49       get_local $var0
# 0x4b       if
# 0x4d         get_local $var0
# 0x4f         i32.const 1
# 0x51         i32.sub
# 0x52         set_local $var0
# 0x54         i32.const 1024
# 0x57         call $func0
# 0x59         br $label0
# 0x5b       end
# 0x5c     end $label0
#   )
# )
# 0x5e   [name]

MEM_MIN = 32
MEM_MAX = 128
FUNC0_START_ADDR = 0x42
FUNC1_RETURN_ADDR = 0x59
FUNC1_START_ADDR = 0x47
