// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(makeSig([], []));
let cont = builder.addCont(sig_v_v);
let tag = builder.addTag(kSig_v_v);
let suspend = builder.addFunction("suspend", kSig_v_v)
  .addBody([
    kExprSuspend, tag
  ]).exportFunc();
builder.addFunction("new_stack", makeSig([], []))
  .addBody([
      kExprRefNull, kExnRefCode,
      kExprRefFunc, suspend.index,
      kExprContNew, cont,
      kExprResumeThrowRef, cont, 0,
  ])
  .exportFunc();
builder.addFunction("suspended_stack", makeSig([], []))
  .addBody([
    kExprRefNull, kExnRefCode,
    kExprBlock, kWasmRefNull, cont,
      kExprRefNull, kExnRefCode,
      kExprRefFunc, suspend.index,
      kExprContNew, cont,
      kExprResume, cont, 1, kOnSuspend, tag, 0,
      kExprUnreachable,
    kExprEnd,
    kExprResumeThrowRef, cont, 0,
  ])
  .exportFunc();
const instance = builder.instantiate();
assertTraps(kTrapRethrowNull, instance.exports.new_stack);
assertTraps(kTrapRethrowNull, instance.exports.suspended_stack);
