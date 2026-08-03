// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-validation --no-wasm-native-module-cache --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// This regression test creates a module with a main function containing a
// call_ref.
// First, feedback is collected in Liftoff, then tier-up to Turbofan is
// triggered. The module including the optimized Turbofan code for main is
// serialized and deserialized. The optimized code gets installed on the
// deserialized native module (the native module cache needs to be disabled to
// get a new instance).
// Now when running the main function with a different call target, a
// deoptimization is triggered which would run into a DCHECK that the function
// (the main function) we are deopting has already been validated.

let builder = new WasmModuleBuilder();
let calleeSig = builder.addType(kSig_i_ii);

builder.addFunction("add", calleeSig).addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  kExprI32Add,
]).exportFunc();

builder.addFunction("mul", calleeSig).addBody([
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  kExprI32Mul,
]).exportFunc();

let mainSig = makeSig([kWasmI32, wasmRefType(calleeSig)], [kWasmI32]);
builder.addFunction("main", mainSig).addBody([
  kExprI32Const, 12,
  kExprLocalGet, 0,
  kExprLocalGet, 1,
  kExprCallRef, calleeSig
]).exportFunc();

let wasmBytes = builder.toBuffer();
let wasmModule = new WebAssembly.Module(wasmBytes);
let wasmExports = new WebAssembly.Instance(wasmModule).exports;
wasmExports.main(30, wasmExports.add);
%WasmTierUpFunction(wasmExports.main);

let serializedModule = d8.wasm.serializeModule(wasmModule);
wasmModule = d8.wasm.deserializeModule(serializedModule, wasmBytes);
let deserializedExports = new WebAssembly.Instance(wasmModule).exports;
deserializedExports.main(30, deserializedExports.mul);
