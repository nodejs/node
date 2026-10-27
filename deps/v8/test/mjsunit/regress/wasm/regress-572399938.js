// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared --expose-gc --verify-heap --allow-natives-syntax

// Tests that we don't unshare strings when invoking `extern.convert_any` for a
// shared input, both in constant expressions and in functions.
// Also that said strings *are* unshared when crossing the Wasm->JS boundary
// as shared externrefs.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();

const struct_type = builder.addStruct({
  fields: [makeField(wasmRefNullType(kWasmExternRef).shared(), true)],
  is_shared: true
});

builder.addImportedGlobal("m", "g", wasmRefNullType(kWasmAnyRef).shared(),
                          false, false);

builder.addGlobal(wasmRefNullType(struct_type), false, false, [
  kExprGlobalGet, 0,
  kGCPrefix, kExprExternConvertAny,
  kGCPrefix, kExprStructNew, struct_type
]).exportAs("global");

builder.addFunction("make_struct", makeSig([], [wasmRefType(struct_type)]))
  .addBody([
    kExprGlobalGet, 0,
    kGCPrefix, kExprExternConvertAny,
    kGCPrefix, kExprStructNew, struct_type])
  .exportFunc();

builder.addFunction("get", makeSig(
    [wasmRefNullType(struct_type)], [wasmRefNullType(kWasmExternRef).shared()]))
  .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct_type, 0])
  .exportFunc();

const js_string = "hello" + " world";
let wasm = builder.instantiate({m: {g: js_string}}).exports;
let struct = wasm.make_struct();
gc();

assertFalse(%IsSharedString(wasm.get(struct)));
assertFalse(%IsSharedString(wasm.get(wasm.global.value)));
