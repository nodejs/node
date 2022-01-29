// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging --wasm-dynamic-tiering --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 10);
builder.addFunction('load', kSig_i_i).addBody([
  // signature: i_i
  // body:
  kExprLocalGet, 0,       // local.get
  kExprI32LoadMem, 0, 0,  // i32.load_mem
]).exportFunc();
const instance = builder.instantiate();
// Call multiple times to trigger dynamic tiering.
while (%IsLiftoffFunction(instance.exports.load)) {
  instance.exports.load(1);
}
