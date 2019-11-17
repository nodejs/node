// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addMemory(1, undefined, false);
builder.addFunction('load', kSig_i_ii)
    .addBody([
        kExprLocalGet, 0,
        kExprI64SConvertI32,
        kExprLocalGet, 1,
        kExprI64SConvertI32,
        kExprI64Shl,
        kExprI32ConvertI64,
    kExprI32LoadMem, 0, 0])
    .exportFunc();

const module = builder.instantiate();
let start = 12;
let address = start;
for (i = 0; i < 64; i++) {
  // This is the address which will be accessed in the code. We cannot use
  // shifts to calculate the address because JS shifts work on 32-bit integers.
  print(`address=${address}`);
  if (address < kPageSize) {
    assertEquals(0, module.exports.load(start, i));
  } else {
    assertTraps(kTrapMemOutOfBounds, _ => { module.exports.load(start, i);});
  }
  address = (address * 2) % 4294967296;
}
