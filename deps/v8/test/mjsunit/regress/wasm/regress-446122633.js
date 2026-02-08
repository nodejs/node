// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let $si = builder.addStruct([makeField(kWasmI32, true)]);
let $src = builder.addStruct([makeField(kWasmExternRef, true), makeField(kWasmI32, true)]);
let $dst = builder.addStruct([makeField(kWasmI32, true), makeField(wasmRefNullType($si), true)]);
let $sig_v_i = builder.addType(kSig_v_i);                               // conv

builder.startRecGroup();

// (type $s0 (sub (descriptor $s0_desc (struct))))
// (type $s0_desc (sub (describes $s0 (struct))))
let $s0_desc = builder.nextTypeIndex() + 1;
let $s0 = builder.addStruct({fields: [], descriptor: $s0_desc});
/* s0_desc */ builder.addStruct({fields: [], describes: $s0});

builder.endRecGroup();

// Two different descs
let $g_desc0 = builder.addGlobal(wasmRefType($s0_desc).exact(), true, false,
  [kGCPrefix, kExprStructNewDefault, $s0_desc]);
let $g_desc1 = builder.addGlobal(wasmRefType($s0_desc).exact(), true, false,
  [kGCPrefix, kExprStructNewDefault, $s0_desc]);

// Aliased src & dst for type confusion.
let $g_src = builder.addGlobal(wasmRefNullType($src), true);
let $g_dst = builder.addGlobal(wasmRefNullType($dst), true);

// Unreachable when analyzed by Turboshaft.
let unreachable = [
  kExprBlock, kAnyRefCode,
    // Instantiate a struct with descriptor $desc0.
    kExprGlobalGet, $g_desc0.index,
    kGCPrefix, kExprStructNewDefault, $s0,

    // Check if this is a struct w/ descriptor $desc1, which is false.
    // WasmGCTypeAnalyzer::ProcessBranchOnTarget assumes otherwise and marks
    // target as unreached.
    kExprGlobalGet, $g_desc1.index,
    ...wasmBrOnCastDescFail(0, wasmRefType($s0), wasmRefType($s0)),

    // make other fallthroughs unreachable
    kExprUnreachable,
  kExprEnd,
  kExprDrop,

  // now anything after this is unreachable.
];

builder.addFunction('conv', $sig_v_i).addLocals(kWasmI32, 1).addLocals(kWasmAnyRef, 1).addBody([
  // conv() -> tier-up runs, skip
  kExprLocalGet, 0,
  kExprI32Eqz,
  kExprBrIf, 0,

  // conv(nonzero) -> tier-up call, confuse
  ...unreachable,

  // now exploit skipped loop reprocessing

  // local1 = 2
  ...wasmI32Const(2),
  kExprLocalSet, 1,

  // init confusion object
  kGCPrefix, kExprStructNewDefault, $src,
  kExprGlobalSet, $g_src.index,

  // local2 : ref exact $dst
  kGCPrefix, kExprStructNewDefault, $dst,
  kExprLocalSet, 2,

  kExprLoop, kWasmVoid,
    kExprLoop, kWasmVoid,
      // if (local1 == 1)
      kExprLocalGet, 1,
      ...wasmI32Const(1),
      kExprI32Sub,
      kExprI32Eqz,
      kExprIf, kWasmVoid,
        // g_dst = Cast<ref $dst>(local2);
        // type still tracked as local2 : ref exact $dst, cast elided
        kExprLocalGet, 2,
        kGCPrefix, kExprRefCast, $dst,
        kExprGlobalSet, $g_dst.index,
      kExprEnd,

      // if (local1 == 3) continue;
      kExprLocalGet, 1,
      ...wasmI32Const(3),
      kExprI32Sub,
      kExprI32Eqz,
      kExprBrIf, 0,

      // break;
    kExprEnd,

    // local2 type updated, but loop reprocessing bypassed due to false static unreachability
    kExprGlobalGet, $g_src.index,
    kExprLocalSet, 2,

    // if (--local) continue;
    kExprLocalGet, 1,
    ...wasmI32Const(1),
    kExprI32Sub,
    kExprLocalTee, 1,
    kExprBrIf, 0,

    // break;
  kExprEnd,
]).exportFunc();

let instance = builder.instantiate();

%WasmTierUpFunction(instance.exports.conv);

assertTraps(kTrapIllegalCast, () => instance.exports.conv(1));
