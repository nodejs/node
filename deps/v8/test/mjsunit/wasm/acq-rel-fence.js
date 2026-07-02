// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-acquire-release

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();

builder.addFunction("atomic_fence_acqrel", kSig_v_v)
  .addBody([
    kAtomicPrefix, kExprAtomicFence, kAtomicAcqRel,
  ])
  .exportFunc();

builder.addFunction("atomic_fence_seqcst", kSig_v_v)
  .addBody([
    kAtomicPrefix, kExprAtomicFence, kAtomicSeqCst,
  ])
  .exportFunc();

let instance = builder.instantiate();
let exports = instance.exports;

exports.atomic_fence_acqrel();
exports.atomic_fence_seqcst();
