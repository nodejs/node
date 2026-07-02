// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref --allow-natives-syntax
// Flags: --wasm-random-rescheduling

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let array = builder.addArray(kWasmI32, {final: true});

builder.addFunction('createArray', makeSig([], [wasmRefType(array)]))
  .exportAs('createArray')
  .addBody([
    kExprI32Const, 5,
    kGCPrefix, kExprArrayNewDefault, array,
  ]);

let main = builder.addFunction('main', makeSig([kWasmStringRef, kWasmStringRef, wasmRefType(array)], []))
  .exportAs('main')
  .addBody([
    // This should trap first with RangeError (Invalid string length) if the strings are too long.
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    ...GCInstr(kExprStringConcat),
    kExprDrop,
    // This should trap second with ArrayOutOfBounds (if it is reached).
    kExprLocalGet, 2,
    kExprI32Const, 10,
    kGCPrefix, kExprArrayGet, array,
    kExprDrop,
  ]);

const instance = builder.instantiate();
const wasm = instance.exports;

// kMaxLength is 1<<20 = 1048576 in lower limits mode.
const len = %StringMaxLength() / 2 + 100;
const s1 = "a".repeat(len);
const s2 = "b".repeat(len);
const a = wasm.createArray();

assertThrows(() => wasm.main(s1, s2, a), RangeError, /Invalid string length/);
%WasmTierUpFunction(wasm.main);
assertThrows(() => wasm.main(s1, s2, a), RangeError, /Invalid string length/);
