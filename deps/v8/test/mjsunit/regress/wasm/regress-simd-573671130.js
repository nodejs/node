// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-revectorize --allow-natives-syntax

// Regression test: IntersectPackNode is only supported when intersecting
// with a ForcePackNode. SLPTree::BuildTreeRec checked node1 for an
// incompatible PackNode only in the `!is_intersected` branch, so when
// node0 alone already set `is_intersected`, node1 could keep a
// DefaultPackNode that doesn't support intersecting, causing a crash
// during revectorization.
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.addFunction('test', kSig_v_v)
  .addLocals(kWasmS128, 6)
  .addBody([
    // load1 = v128.load(0, offset=0), load2 = v128.load(0, offset=16)
    kExprI32Const, 0, kSimdPrefix, kExprS128LoadMem, 4, 0, kExprLocalSet, 0,
    kExprI32Const, 0, kSimdPrefix, kExprS128LoadMem, 4, 16, kExprLocalSet, 1,
    // u1 = i8x16.abs(load1) (emitted before load0)
    kExprLocalGet, 0, kSimdPrefix, kExprI8x16Abs, kExprLocalSet, 2,
    // load0 = v128.load(0, offset=64), load3 = v128.load(0, offset=96)
    kExprI32Const, 0, kSimdPrefix, kExprS128LoadMem, 4, 64, kExprLocalSet, 3,
    kExprI32Const, 0, kSimdPrefix, kExprS128LoadMem, 4, 96, kExprLocalSet, 4,
    // u0 = i8x16.abs(load0)
    kExprLocalGet, 3, kSimdPrefix, kExprI8x16Abs, kExprLocalSet, 5,
    // Tree 1: packs [load1, load2] as DefaultPackNode
    kExprI32Const, 0, kExprLocalGet, 0, kSimdPrefix, kExprI8x16Neg,
    kSimdPrefix, kExprS128Not, kSimdPrefix, kExprS128StoreMem, 4, ...wasmUnsignedLeb(128),
    kExprI32Const, 0, kExprLocalGet, 1, kSimdPrefix, kExprI8x16Neg,
    kSimdPrefix, kExprS128Not, kSimdPrefix, kExprS128StoreMem, 4, ...wasmUnsignedLeb(144),
    // Tree 2: packs [load0, load3] as ForcePackNode
    kExprI32Const, 0, kExprLocalGet, 3, kSimdPrefix, kExprI8x16Neg,
    kSimdPrefix, kExprS128Not, kSimdPrefix, kExprS128StoreMem, 4, ...wasmUnsignedLeb(160),
    kExprI32Const, 0, kExprLocalGet, 4, kSimdPrefix, kExprI8x16Neg,
    kSimdPrefix, kExprS128Not, kSimdPrefix, kExprS128StoreMem, 4, ...wasmUnsignedLeb(176),
    // Tree 3: packs [u0, u1] and creates IntersectPackNode([load0, load1])
    kExprI32Const, 0, kExprLocalGet, 5, kSimdPrefix, kExprI8x16Neg,
    kSimdPrefix, kExprS128StoreMem, 4, ...wasmUnsignedLeb(192),
    kExprI32Const, 0, kExprLocalGet, 2, kSimdPrefix, kExprI8x16Neg,
    kSimdPrefix, kExprS128StoreMem, 4, ...wasmUnsignedLeb(208),
  ]).exportFunc();
const instance = builder.instantiate();
instance.exports.test();
%WasmTierUpFunction(instance.exports.test);
instance.exports.test();
