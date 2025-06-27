// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --liftoff --wasm-trap-handler

// A simple test to make sure Liftoff can compile memory operations with trap
// handlers enabled.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function testCompileLoadStore() {
  const builder = new WasmModuleBuilder();
  // These functions generate statically out of bounds accesses.
  builder.addFunction("load", kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprI32LoadMem, 0, 0x80, 0x80, 0x80, 1])
      .exportFunc();
  builder.addFunction("store", kSig_i_ii)
      .addBody([kExprLocalGet, 0,
                kExprLocalGet, 1,
                kExprI32StoreMem, 0, 0x80, 0x80, 0x80, 1,
                kExprLocalGet, 1])
      .exportFunc();
  builder.addMemory(1, 1);
  const instance = builder.instantiate();
}
testCompileLoadStore();
