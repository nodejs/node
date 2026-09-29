// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --trace-turbo

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig_index = builder.addType(kSig_v_v);
builder.addImport('e', 'f', sig_index);
builder.addExport('f', 0); // Export function 0 (the imported function)

const instance = builder.instantiate({
  e: { f: function() {} }
});

const f = instance.exports.f;

function test() {
  return f();
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
test();
