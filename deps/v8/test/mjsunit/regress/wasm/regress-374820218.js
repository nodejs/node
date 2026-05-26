// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function() {

let builder = new WasmModuleBuilder();
builder.addFunction("main", kSig_i_i)
      .addBody([kExprLocalGet, 0])
      .exportAs("main");

let bytes = builder.toBuffer();
let bytesPromise = Promise.resolve(bytes);

// Promise species shouldn't affect the Wasm compilation promise.
let oldPromiseSpecies = Object.getOwnPropertyDescriptor(Promise, Symbol.species);
Object.defineProperty(Promise, Symbol.species, {value: -13});

assertInstanceof(WebAssembly.compileStreaming(bytesPromise), Promise);

// Restore the promise species for test cleanup.
Object.defineProperty(Promise, Symbol.species, oldPromiseSpecies);

})();
