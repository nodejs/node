// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --nowasm-loop-unrolling --nowasm-loop-peeling

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js")

const builder = new WasmModuleBuilder();

const arr = builder.addArray(kWasmI32, { final: true });
const holder = builder.addStruct([makeField(wasmRefType(arr), true)]);

const bigLen = 8;
const smallLen = 1;
const victimInit = 0x12345678;
const expandedVictimLen = 0x40;

const gVictim = builder.addGlobal(wasmRefNullType(arr), true);
const gHolder = builder.addGlobal(wasmRefNullType(holder), true);

builder.addFunction('expand_victim_length', makeSig([], []))
  .addLocals(wasmRefType(holder), 1)
  .addLocals(wasmRefType(arr), 3)
  .addLocals(kWasmI32, 1)
  .addBody([
    kExprI32Const, bigLen,
    kGCPrefix, kExprArrayNewDefault, arr,
    kExprLocalSet, 1,

    kExprI32Const, smallLen,
    kGCPrefix, kExprArrayNewDefault, arr,
    kExprLocalSet, 2,

    kExprI32Const, smallLen,
    kGCPrefix, kExprArrayNewDefault, arr,
    kExprLocalSet, 3,

    kExprLocalGet, 3,
    kExprI32Const, 0,
    ...wasmI32Const(victimInit),
    kGCPrefix, kExprArraySet, arr,

    kExprLocalGet, 1,
    kGCPrefix, kExprStructNew, holder,
    kExprLocalSet, 0,

    kExprLocalGet, 3,
    kExprGlobalSet, gVictim.index,
    kExprLocalGet, 0,
    kExprGlobalSet, gHolder.index,

    kExprI32Const, 2,
    kExprLocalSet, 4,

    kExprLoop, kWasmVoid,
    kExprLocalGet, 4,
    kExprI32Const, 2,
    kExprI32Eq,
    kExprIf, kWasmRef, arr,
    kExprLocalGet, 1,
    kExprElse,
    kExprLocalGet, 0,
    kGCPrefix, kExprStructGet, holder, 0,
    kExprEnd,
    kExprI32Const, 3,
    kExprI32Const, expandedVictimLen,
    kGCPrefix, kExprArraySet, arr,

    kExprLocalGet, 0,
    kExprLocalGet, 2,
    kGCPrefix, kExprStructSet, holder, 0,

    kExprLocalGet, 4,
    kExprI32Const, 1,
    kExprI32Sub,
    kExprLocalTee, 4,
    kExprBrIf, 0,
    kExprEnd,
  ])
  .exportFunc();

builder.addFunction('victim_write', kSig_v_ii)
  .addBody([
    kExprGlobalGet, gVictim.index,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kGCPrefix, kExprArraySet, arr,
  ])
  .exportFunc();

builder.addFunction('get_holder', kSig_r_v)
  .addBody([
    kExprGlobalGet, gHolder.index,
    kGCPrefix, kExprStructGet, holder, 0,
    kGCPrefix, kExprExternConvertAny,
  ])
  .exportFunc();

const instance = builder.instantiate({});
const wasm = instance.exports;

assertTraps(kTrapArrayOutOfBounds, () => {
  wasm.expand_victim_length()
  let addr = 0x10000001;
  wasm.victim_write(3, addr);
  wasm.get_holder();
});
