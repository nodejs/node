// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --turbofan --trace-turbo-graph

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addMemory(1, 4);
builder.addFunction("grow", kSig_i_i)
.addBody([
  kExprLocalGet, 0,
  kExprMemoryGrow, kMemoryZero,
]).exportFunc();

// CHECK: Constant()[relocatable wasm stub call: WasmMemoryGrow (0x{{[0-9A-Fa-f]+}})]
let instance = builder.instantiate().exports.grow(1);
