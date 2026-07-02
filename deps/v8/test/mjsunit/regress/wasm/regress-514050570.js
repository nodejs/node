// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let tiny = builder.addFunction("tiny", kSig_v_v).addBody([]);

// b1 calls tiny 60 times.
let b1_body = [];
for (let i = 0; i < 60; i++) {
  b1_body.push(kExprCallFunction, tiny.index);
}
let b1 = builder.addFunction("b1", kSig_v_v).addBody(b1_body);

let extra = builder.addFunction("extra", kSig_v_v).addBody([]);

// main calls b1, then extra 100 times.
let main_body = [kExprCallFunction, b1.index];
for (let i = 0; i < 100; i++) {
  main_body.push(kExprCallFunction, extra.index);
}
let main = builder.addFunction("main", kSig_v_v)
  .addBody(main_body)
  .exportFunc();

// Set hints for main.
let hints = [];
hints.push({offset: 1, frequency: 127});
for (let i = 0; i < 100; i++) {
  hints.push({offset: 3 + i * 2, frequency: 64});
}
builder.setInstructionFrequencies(main.index, hints);
builder.setCompilationPriority(main.index, 0, 0);

let instance = builder.instantiate();
instance.exports.main();
