// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-inline-js-wasm-calls

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Instantiate a module that exports an imported function
// (module
//   (func $import0 (import "e" "f"))
//   (export "f" (func $import0))
// )
const builder = new WasmModuleBuilder();
const sig_index = builder.addType(kSig_v_v);
builder.addImport('e', 'f', sig_index);
builder.addExport('f', 0);
let instance = builder.instantiate(
    {e: {f: function() {}}}
);

// Invoke the JS function exported by the Wasm module
const f = instance.exports['f'];
function invoke_vv(index) {
  return f();
}

%PrepareFunctionForOptimization(invoke_vv);
invoke_vv(0);
%OptimizeFunctionOnNextCall(invoke_vv);
invoke_vv(0);
