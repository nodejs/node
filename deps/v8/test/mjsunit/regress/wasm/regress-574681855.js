// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-deopt --wasm-inlining-ignore-call-counts
// Flags: --wasm-sync-tier-up --wasm-inlining-budget=25000
// Flags: --wasm-inlining-max-size=2500 --wasm-inlining-factor=15

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig_index = builder.addType(makeSig(
    [
      kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32,
      kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32,
      kWasmI32, kWasmI32
    ],
    [kWasmI32]));

const g = builder.addGlobal(kWasmI32, true, false);
for (let d = 0; d < 10; d++) {
  builder.addFunction('dummy_' + d, kSig_i_i).addBody([kExprLocalGet, 0]);
}

// Keep leaf larger than --wasm-inlining-max-size so it remains a real call that
// clobbers caller-saved registers and splits live ranges across branches.
let leaf_body = [kExprLocalGet, 0];
for (let i = 0; i < 900; i++) {
  leaf_body.push(kExprI32Const, 1, kExprI32Add);
}
const leaf = builder.addFunction('leaf', kSig_i_i).addBody(leaf_body);

const num_callees = 3;
const callees = [];

for (let c = 0; c < num_callees; c++) {
  let body = [];
  for (let iter = 0; iter < 25; iter++) {
    body.push(
        kExprLocalGet, iter % 16,
        kExprIf, kWasmVoid,
          kExprLocalGet, (iter + 1) % 16,
          kExprCallFunction, leaf.index,
          kExprLocalGet, (iter + 2) % 16,
          kExprI32Add,
          kExprLocalSet, (iter + 3) % 16,
        kExprElse,
          kExprLocalGet, (iter + 4) % 16,
          kExprLocalGet, (iter + 5) % 16,
          kExprI32Mul,
          kExprLocalSet, (iter + 6) % 16,
        kExprEnd);
  }
  body.push(kExprLocalGet, 0);
  for (let i = 1; i < 16; i++) {
    body.push(kExprLocalGet, i, kExprI32Add);
  }
  callees.push(builder.addFunction('callee_' + c, sig_index).addBody(body));
}

const table = builder.addTable(kWasmFuncRef, num_callees, num_callees);
builder.addActiveElementSegment(
    table.index, wasmI32Const(0),
    callees.map(f => [kExprRefFunc, f.index]), kWasmFuncRef);

let inlinee_body = [];
for (let k = 0; k < 6; k++) {
  for (let i = 0; i < 16; i++) {
    inlinee_body.push(kExprLocalGet, (i + k) % 16);
  }
  inlinee_body.push(
      kExprGlobalGet, g.index,
      kExprCallIndirect, sig_index, table.index,
      kExprLocalSet, k % 16);
}
inlinee_body.push(kExprLocalGet, 0);
const inlinee =
    builder.addFunction('inlinee', sig_index).addBody(inlinee_body).exportFunc();

let main_body = [
  kExprLocalGet, 0,
  kExprGlobalSet, g.index,
];
for (let i = 0; i < 16; i++) {
  main_body.push(kExprLocalGet, 0, kExprI32Const, i + 1, kExprI32Add);
}
main_body.push(kExprCallFunction, inlinee.index);
builder.addFunction('main', kSig_i_i).addBody(main_body).exportFunc();

const instance = builder.instantiate();
for (let i = 0; i < num_callees; i++) {
  instance.exports.main(i);
  %WasmTierUpFunction(instance.exports.inlinee);
}
