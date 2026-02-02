// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-jspi --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let kSig_i_rr = makeSig([kWasmExternRef, kWasmExternRef], [kWasmI32]);

let builder = new WasmModuleBuilder();
let string_equals = builder.addImport('wasm:js-string', 'equals', kSig_i_rr);

builder.addFunction("eq", kSig_i_rr)
  .exportFunc()
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprCallFunction, string_equals,
  ]);

let instance = builder.instantiate({}, { builtins: ["js-string"] });

assertPromiseResult(
    WebAssembly.promising(instance.exports.eq)(
    %ConstructConsString('abcdefghijklm', 'abcdefghijkl'),
    %ConstructConsString('abcdefghijklm', 'abcdefghijkl')),
    v => assertEquals(v, 1));
