// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);
builder.addFunction(undefined, kSig_v_v).addBody([
  kExprI32Const, 0,  // i32.const 0
  kExprI64LoadMem, 0, 0xff, 0xff, 0xff, 0xff,
  0x0f,       // i64.load align=0 offset=0xffffffff
  kExprDrop,  // drop
]);
builder.addExport('main', 0);
const module = builder.instantiate();
assertThrows(
    () => module.exports.main(), WebAssembly.RuntimeError, /out of bounds/);
