// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-growable-stacks --no-wasm-inlining
// Flags: --wasm-wasmfx --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function addActiveLocals(func, numParams, numLocals = 600) {
  func.addLocals(kWasmI64, numLocals + 1);
  const firstLocal = numParams;
  const lastLocal = numParams + numLocals - 1;
  const sumIdx = numParams + numLocals;
  const body = [
    kExprLocalGet, 0,
    kExprI64UConvertI32,
    kExprI64Const, 1,
    kExprI64Add,
    kExprLocalSet, ...wasmUnsignedLeb(firstLocal),
  ];
  for (let i = firstLocal + 1; i <= lastLocal; ++i) {
    body.push(
        kExprLocalGet, ...wasmUnsignedLeb(i - 1),
        kExprI64Const, 1,
        kExprI64Add,
        kExprLocalSet, ...wasmUnsignedLeb(i),
    );
  }
  body.push(kExprI64Const, 0, kExprLocalSet, ...wasmUnsignedLeb(sumIdx));
  for (let i = firstLocal; i <= lastLocal; ++i) {
    body.push(
        kExprLocalGet, ...wasmUnsignedLeb(sumIdx),
        kExprLocalGet, ...wasmUnsignedLeb(i),
        kExprI64Add,
        kExprLocalSet, ...wasmUnsignedLeb(sumIdx),
    );
  }
  body.push(
      kExprLocalGet, ...wasmUnsignedLeb(sumIdx),
      kExprI64Eqz,
      kExprIf, kWasmVoid,
      kExprUnreachable,
      kExprEnd,
  );
  return body;
}

const builder = new WasmModuleBuilder();

// 12 parameters (forces stack parameters across IA32, X64, ARM, and ARM64)
const paramTypes = Array(12).fill(kWasmI32);
const sig = builder.addType(makeSig(paramTypes, [kWasmI32]));

// f_dest: adds up all 12 parameters when depth == 0
const f_dest_body = [
  kExprLocalGet, 0,  // depth
  kExprI32Eqz,
  kExprIf, kWasmI32,
];
f_dest_body.push(kExprLocalGet, 0);
for (let i = 1; i < 12; ++i) {
  f_dest_body.push(kExprLocalGet, i, kExprI32Add);
}
f_dest_body.push(kExprReturn, kExprElse);
// Otherwise decrement depth and call f_grow
f_dest_body.push(
    kExprLocalGet, 0,
    kExprI32Const, 1,
    kExprI32Sub,
);
for (let i = 1; i < 12; ++i) {
  f_dest_body.push(kExprLocalGet, i);
}

const f_dest = builder.addFunction('f_dest', sig);
const f_grow = builder.addFunction('f_grow', sig);

f_dest_body.push(kExprCallFunction, f_grow.index, kExprEnd);
f_dest.addBody(f_dest_body);

// f_grow: large frame (600 locals) to force stack growth, then tail calls
// f_dest.
const f_grow_body = [...addActiveLocals(f_grow, 12, 600)];
for (let i = 0; i < 12; ++i) {
  f_grow_body.push(kExprLocalGet, i);
}
f_grow_body.push(kExprReturnCall, f_dest.index);
f_grow.addBody(f_grow_body);

builder.addExport('main', f_dest.index);

const instance = builder.instantiate();
const promising_main = WebAssembly.promising(instance.exports.main);

// Sum of 1..11 = 66
// First arg is the depth
assertPromiseResult(promising_main(5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
  res => assertEquals(66, res));
