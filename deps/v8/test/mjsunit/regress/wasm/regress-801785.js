// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Flags: --print-wasm-code

const builder = new WasmModuleBuilder();
builder.addMemory(8, 16);
builder.addFunction(undefined, kSig_i_i).addBody([
  // wasm to wasm call.
  kExprLocalGet, 0, kExprCallFunction, 0x1
]);
builder.addFunction(undefined, kSig_i_i).addBody([
  // load from <get_local 0> to create trap code.
  kExprLocalGet, 0, kExprI32LoadMem, 0,
  // unreachable to create a runtime call.
  kExprUnreachable
]);
builder.instantiate();
