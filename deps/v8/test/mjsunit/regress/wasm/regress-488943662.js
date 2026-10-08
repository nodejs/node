// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-code-coverage --expose-gc --no-wasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function foo() {}

let wire_bytes = (() => {
  var builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_v_v).addBody([]).exportFunc();
  return builder.toBuffer();
})();

let serialized_bytes =
    d8.wasm.serializeModule(new WebAssembly.Module(wire_bytes));

try {
  foo.toString();
  gc();
  let module = d8.wasm.deserializeModule(serialized_bytes, wire_bytes);
  var instance = new WebAssembly.Instance(module);
  instance.exports.main();
} catch (e) {}
