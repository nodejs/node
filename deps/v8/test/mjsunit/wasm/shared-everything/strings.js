// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function SharedStringConstants() {
  let builder = new WasmModuleBuilder();

  let kSharedRefExtern = wasmRefType(kWasmExternRef).shared();

  let struct = builder.addStruct({fields: [makeField(kSharedRefExtern, true)],
                                  shared: true})

  let foo = builder.addImportedGlobal(
      "'", "foo", wasmRefType(kWasmExternRef), false);
  let shared_foo = builder.addImportedGlobal(
      "'", "shared_foo", kSharedRefExtern, false);
  let one_char_string = builder.addImportedGlobal(
      "'", "o", kSharedRefExtern, false);
  let empty_string = builder.addImportedGlobal(
      "'", "", kSharedRefExtern, false);

  builder.addFunction("get_foo", kSig_r_v)
    .addBody([kExprGlobalGet, foo])
    .exportFunc();
  builder.addFunction("get_shared_foo", makeSig([], [kSharedRefExtern]))
    .addBody([kExprGlobalGet, shared_foo])
    .exportFunc();
  builder.addFunction("get_one_char_string", makeSig([], [kSharedRefExtern]))
    .addBody([kExprGlobalGet, one_char_string])
    .exportFunc();
  builder.addFunction("get_empty_string", makeSig([], [kSharedRefExtern]))
    .addBody([kExprGlobalGet, empty_string])
    .exportFunc();

  builder.addFunction("struct_new", makeSig([kSharedRefExtern],
                                            [wasmRefType(struct)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  builder.addFunction("struct_get", makeSig([wasmRefType(struct)],
                                            [kSharedRefExtern]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  builder.addFunction("struct_new_internal", makeSig([], [wasmRefType(struct)]))
    .addBody([kExprGlobalGet, shared_foo, kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  builder.addFunction("struct_new_internal_empty",
                      makeSig([], [wasmRefType(struct)]))
    .addBody([kExprGlobalGet, empty_string, kGCPrefix, kExprStructNew, struct])
    .exportFunc();

  let instance = builder.instantiate({}, {importedStringConstants: "'"});

  let wasm = instance.exports;

  assertEquals("foo", wasm.get_foo());
  assertEquals("shared_foo", wasm.get_shared_foo());
  assertEquals("o", wasm.get_one_char_string());
  assertEquals("", wasm.get_empty_string());

  // Make sure strings get unshared at the Wasm-JS boundary.
  assertFalse(%IsSharedString(wasm.get_foo()));
  assertFalse(%IsInWritableSharedSpace(wasm.get_foo()));
  assertFalse(%IsSharedString(wasm.get_shared_foo()));
  assertFalse(%IsInWritableSharedSpace(wasm.get_shared_foo()));
  assertFalse(%IsSharedString(wasm.get_one_char_string()));
  assertFalse(%IsInWritableSharedSpace(wasm.get_one_char_string()));
  assertFalse(%IsSharedString(wasm.get_empty_string()));
  assertFalse(%IsInWritableSharedSpace(wasm.get_empty_string()));

  // Make sure a JS string gets shared when entering Wasm as a shared
  // externref and can be assigned to a shared object.
  assertEquals("bar", wasm.struct_get(wasm.struct_new("bar")));
  // Make sure a shared Wasm string can be assigned to a shared object...
  assertEquals("shared_foo", wasm.struct_get(wasm.struct_new_internal()));
  // ... but when it exits to JS it is not shared.
  assertFalse(%IsSharedString(wasm.struct_get(wasm.struct_new_internal())));
  assertFalse(%IsInWritableSharedSpace(
      wasm.struct_get(wasm.struct_new_internal())));
  // Same for the empty string.
  assertEquals("", wasm.struct_get(wasm.struct_new_internal_empty()));
  assertFalse(%IsSharedString(wasm.struct_get(
      wasm.struct_new_internal_empty())));
  assertFalse(%IsInWritableSharedSpace(
      wasm.struct_get(wasm.struct_new_internal_empty())));
})();
