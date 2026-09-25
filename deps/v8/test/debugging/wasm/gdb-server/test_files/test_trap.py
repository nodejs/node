# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# 0x00 (module
# 0x08   [type]
# 0x0a   (type $type0 (func))
# 0x0e   (type $type1 (func (result i32)))
# 0x12   [function]
# 0x17   [memory]
# 0x19   (memory $memory0 32 128)
# 0x1e   [export]
# 0x20   (export "mem" (memory $memory0))
# 0x27   (export "main" (func $func1))
# 0x2e   [code]
# 0x30   (func $func0
# 0x33     i32.const 0
# 0x35     i32.const 42
# 0x37     i32.store offset=-1 align=1
# 0x3e   )
# 0x3f   (func $func1 (result i32)
# 0x41     call $func0
# 0x43     i32.const 0
# 0x45   )
# 0x46   ...
#      )

TRAP_ADDRESS = 0x0037
