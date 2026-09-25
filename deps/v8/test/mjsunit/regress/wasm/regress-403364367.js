// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let import0 = builder.addImport('imports', 'f0', kSig_v_v);
builder.addFunction("wrap_f0", kSig_v_v).exportFunc().addBody([
    kExprCallFunction, import0,
  ]);

function f0() {
  assertEquals(null, f0.caller);
}

f0();
const wasm = builder.instantiate({ imports: { f0 } });
wasm.exports.wrap_f0();
