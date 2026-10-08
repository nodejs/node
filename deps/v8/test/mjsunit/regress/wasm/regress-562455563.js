// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cpu-profiler-sampling-interval=1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Profile while many Wasm functions run the Liftoff feedback vector setup
// builtin for the first time. That builtin sets the WASM_LIFTOFF_SETUP frame
// marker a few instructions before it spills its calling PC, and a tick landing
// in that window used to abort on arm64 with control flow integrity.

const kNumFunctions = 100;

const builder = new WasmModuleBuilder();
const $sig_v_v = builder.addType(kSig_v_v);
// Callee of the unreachable call below, which is what makes the functions
// require a feedback vector in the first place.
const $dummy = builder.addFunction('dummy', $sig_v_v).addBody([]);

let main_body = [];
for (let i = 0; i < kNumFunctions; i++) {
  const callee = builder.addFunction(undefined, $sig_v_v).addBody([
    kExprI32Const, 0,
    kExprIf, kWasmVoid,
      // An unreachable call nevertheless requires a feedback vector entry.
      kExprCallFunction, $dummy.index,
    kExprEnd,
  ]);
  main_body.push(kExprCallFunction, ...wasmUnsignedLeb(callee.index));
}
builder.addFunction('main', $sig_v_v).exportFunc().addBody(main_body);

const instance = builder.instantiate();

// Note: no {console.profileEnd()}, which would dump the profile into a
// "v8.prof" file in the current working directory.
console.profile();
instance.exports.main();
