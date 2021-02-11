// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test needs --wasm-tier-up because we can't serialize and deserialize
// Liftoff code.
// Flags: --expose-wasm --allow-natives-syntax --expose-gc --wasm-tier-up

load("test/mjsunit/wasm/wasm-module-builder.js");


const wire_bytes = new WasmModuleBuilder().toBuffer();

const serialized = (() => {
  return %SerializeWasmModule(new WebAssembly.Module(wire_bytes));
})();

// Collect the compiled module, to avoid sharing of the NativeModule.
gc();

const module = %DeserializeWasmModule(serialized, wire_bytes);
%SerializeWasmModule(module);
