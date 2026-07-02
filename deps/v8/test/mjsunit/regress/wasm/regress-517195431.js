// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --wasm-inlining-min-budget=1000

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

let tiny = builder.addFunction("tiny", kSig_v_v).addBody([]);
let t1_body = [];
for (let i = 0; i < 60; i++) {
  t1_body.push(kExprCallFunction, tiny.index);
}
let t1 = builder.addFunction("t1", kSig_v_v).addBody(t1_body);

let targets_hints = [{function_index: t1.index, frequency_percent: 1}];
for (let i = 2; i <= 60; i++) {
  let t = builder.addFunction("t" + i, kSig_v_v).addBody([]);
  targets_hints.push({function_index: t.index, frequency_percent: 1});
}

let main = builder.addFunction("main", kSig_v_i)
  .addBody([
    kExprLocalGet, 0,
    kExprCallIndirect, 0, 0
  ]);

let table = builder.addTable(kWasmFuncRef, 100, 100);

let hints = [];
hints.push({offset: 3, frequency: 127}); // call_indirect
builder.setInstructionFrequencies(main.index, hints);

let targets = [
  {
    offset: 3,
    targets: targets_hints
  }
];
builder.setCallTargets(main.index, targets);
builder.setCompilationPriority(main.index, 0, 0);

let instance = builder.instantiate();
