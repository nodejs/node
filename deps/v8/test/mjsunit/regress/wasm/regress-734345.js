// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

load('test/mjsunit/wasm/wasm-module-builder.js');

builder1 = new WasmModuleBuilder();
builder1.addFunction('exp1', kSig_v_v).addBody([kExprUnreachable]).exportFunc();

builder2 = new WasmModuleBuilder();
builder2.addImport('imp', 'imp', kSig_v_v);
builder2.addFunction('call_imp', kSig_v_v)
    .addBody([kExprCallFunction, 0])
    .exportFunc();

export1 = builder1.instantiate().exports.exp1;
export2 = builder2.instantiate({imp: {imp: export1}}).exports.call_imp;
export1 = undefined;

let a = [0];
for (i = 0; i < 10; ++i) {
  a = a.concat(new Array(i).fill(i));
  assertThrows(() => export2(), WebAssembly.RuntimeError);
  gc();
}
