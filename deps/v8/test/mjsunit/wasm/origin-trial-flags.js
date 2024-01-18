// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noexperimental-wasm-gc --no-experimental-wasm-stringref
// Flags: --allow-natives-syntax

load("test/mjsunit/wasm/wasm-module-builder.js");

function instantiateModuleWithGC() {
  // Build a WebAssembly module which uses Wasm GC features.
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', makeSig([], [kWasmAnyRef]))
      .addBody([
        kExprI32Const, 42,
        kGCPrefix, kExprRefI31,
      ])
      .exportFunc();

  return builder.instantiate();
}

function instantiateModuleWithStringRef() {
  // Build a WebAssembly module which uses stringref features.
  const builder = new WasmModuleBuilder();
  builder.addFunction("main",
                      makeSig([kWasmStringRef], [kWasmStringRef]))
  .addBody([kExprLocalGet, 0])
  .exportFunc();
  return builder.instantiate();
}

// Due to --noexperimental-wasm-gc GC is disabled.
assertThrows(instantiateModuleWithGC, WebAssembly.CompileError);
// Due to --noexperimental-wasm-stringref stringrefs are not supported.
assertThrows(instantiateModuleWithStringRef, WebAssembly.CompileError);
// Disable WebAssembly GC explicitly.
%SetWasmGCEnabled(false);
assertThrows(instantiateModuleWithGC, WebAssembly.CompileError);
assertThrows(instantiateModuleWithStringRef, WebAssembly.CompileError);
// Enable WebAssembly GC explicitly.
%SetWasmGCEnabled(true);
assertEquals(42, instantiateModuleWithGC().exports.main());
// Enabling Wasm GC via callback will also enable wasm stringref.
let str = "Hello World!";
assertSame(str, instantiateModuleWithStringRef().exports.main(str));
