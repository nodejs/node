// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging --fuzzing

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
const builder = new WasmModuleBuilder();
const func = builder.addImport("m", "i", kSig_v_v);
builder.addFunction("main", kSig_v_v)
       .addBody([kExprCallFunction,func])
       .exportFunc();
const v27 = new WebAssembly.Suspending(() => { return Promise.resolve(); });
const instance = builder.instantiate({"m": {"i": v27}});
function f() {
  delete {}[instance];
}
const pi = WebAssembly.promising(instance.exports.main);
pi();
instance.toString = f;
try {
  instance.toString();
} catch (e) {}
