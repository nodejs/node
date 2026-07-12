// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);
builder.addFunction(undefined, kSig_i_iii).addBody([
  kExprI32Const, 0,         // i32.const 0
  kExprI32LoadMem8S, 0, 0,  // i32.load8_s offset=0 align=0
  kExprI32Eqz,              // i32.eqz
]);
builder.instantiate();
