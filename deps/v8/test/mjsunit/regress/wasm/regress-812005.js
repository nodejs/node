// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_d_v).addBody([
  ...wasmF64Const(0),  // f64.const 0
  ...wasmF64Const(0),  // f64.const 0
  ...wasmI32Const(0),  // i32.const 0
  kExprBrIf, 0x00,     // br_if depth=0
  kExprF64Add          // f64.add
]);
builder.instantiate();
