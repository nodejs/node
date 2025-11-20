// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-growable-stacks
// Flags: --wasm-wrapper-tiering-budget=1

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const func_sig = builder.addType(kSig_i_i);
let fun = builder.addFunction("fun", func_sig);
fun.addBody([kExprLocalGet, 0]).exportFunc();
const numRets = 10;
const retTypes = Array(numRets).fill(kWasmFuncRef);
builder.addFunction("test", makeSig([], retTypes))
  .addBody([
    ...Array(numRets).fill([kExprRefFunc, fun.index]).flat(),
  ]).exportFunc();
const instance = builder.instantiate({});
for (let i = 0; i < 10; i++) {
  instance.exports.test();
}
