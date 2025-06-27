// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function() {
"use asm";
  var builder = new WasmModuleBuilder();
  builder.addFunction("regression_702460", kSig_i_v)
    .addBody([
        kExprI32Const, 0x52,
        kExprI32Const, 0x41,
        kExprI32Const, 0x3c,
        kExprI32Const, 0xdc, 0x01,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprLocalSet, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprMemoryGrow, 0x00,
        kExprS128LoadMem, 0x00, 0x40,
        kExprUnreachable,
        kExprMemoryGrow, 0x00
        ]).exportFunc();
  assertThrows(() => builder.instantiate());
})();
