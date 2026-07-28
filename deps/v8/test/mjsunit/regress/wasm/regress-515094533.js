// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-compilation-hints --wasm-inlining-min-budget=1000

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let t1 = builder.addFunction("t1", kSig_v_v).addBody([]);
let t2 = builder.addFunction("t2", kSig_v_v).addBody([]);
let t3 = builder.addFunction("t3", kSig_v_v).addBody([]);
let t4 = builder.addFunction("t4", kSig_v_v).addBody([]);
let t5 = builder.addFunction("t5", kSig_v_v).addBody([]);
let t6 = builder.addFunction("t6", kSig_v_v).addBody([]);

let tiny = builder.addFunction("tiny", kSig_v_v).addBody([]);

// b1 calls tiny 58 times.
let b1_body = [];
for (let i = 0; i < 58; i++) {
  b1_body.push(kExprCallFunction, tiny.index);
}
let b1 = builder.addFunction("b1", kSig_v_v).addBody(b1_body);

// main calls b1, then call_indirect.
let main = builder.addFunction("main", kSig_v_i)
  .addBody([
    kExprCallFunction, b1.index,
    kExprLocalGet, 0,
    kExprCallIndirect, 0, 0 // type 0, table 0
  ])

let table = builder.addTable(kWasmFuncRef, 10, 10);
let hints = [];
hints.push({offset: 1, frequency: 127}); // b1
hints.push({offset: 5, frequency: 127}); // call_indirect
builder.setInstructionFrequencies(main.index, hints);
let targets = [
  {
    offset: 5,
    targets: [
      {function_index: t1.index, frequency_percent: 16},
      {function_index: t2.index, frequency_percent: 16},
      {function_index: t3.index, frequency_percent: 16},
      {function_index: t4.index, frequency_percent: 16},
      {function_index: t5.index, frequency_percent: 16},
      {function_index: t6.index, frequency_percent: 16}
    ]
  }
];
builder.setCallTargets(main.index, targets);

builder.setCompilationPriority(main.index, 0, 0);

let instance = builder.instantiate();
