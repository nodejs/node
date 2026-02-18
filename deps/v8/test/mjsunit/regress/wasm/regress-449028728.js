// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft-wasm-in-js-inlining --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let global = new WebAssembly.Global({
  value: "i64",
  mutable: true
}, 0n);

const builder = new WasmModuleBuilder();
let $global0 = builder.addImportedGlobal('imports', 'g0', kWasmI64, true);
builder.addFunction("w0", kSig_v_v).addBody([
  kExprGlobalGet, $global0,
  kExprGlobalSet, $global0,
]).exportFunc();

const instance = builder.instantiate({
  imports: {
    g0: global
  }
});

function f() {
  instance.exports.w0();
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
