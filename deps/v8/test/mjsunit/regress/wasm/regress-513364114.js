// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared --allow-natives-syntax
// Flags: --wasm-wrapper-tiering-budget=10

// Optimized JS-to-Wasm export wrapper ToJS should call String::Unshare for
// (ref shared any), because otherwise we can leak a shared string via
// any.convert_extern.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// (ref shared extern), non-nullable
let kSharedRefExtern = wasmRefType(kWasmExternRef).shared();
// (ref shared any), non-nullable
let kSharedRefAny = wasmRefType(kWasmAnyRef).shared();

let builder = new WasmModuleBuilder();

// Immutable imported global of type (ref shared extern). The magic
// string-constants module ("'") will provide a SeqString allocated in the
// writable shared old-space (because the global type is shared).
let g = builder.addImportedGlobal(
    "'", "AAAAAAAAAAAAAAAAAAAA", kSharedRefExtern, /*mutable=*/false);

// This returns a shared string as anyref.
builder.addFunction("any_string", makeSig([], [kSharedRefAny]))
  .addBody([kExprGlobalGet, g, kGCPrefix, kExprAnyConvertExtern])
  .exportFunc();

// CONTROL: returns (ref shared extern).
builder.addFunction("ctrl", makeSig([], [kSharedRefExtern]))
  .addBody([kExprGlobalGet, g])
  .exportFunc();

let instance = builder.instantiate({}, {importedStringConstants: "'"});
let wasm = instance.exports;

function check(fn) {
  let v = fn();
  assertFalse(%IsSharedString(v));
  assertFalse(%IsInWritableSharedSpace(v));
}

print("=== Before tier-up (generic JSToWasm wrapper) ===");
// The generic builtin wrapper (js-to-wasm.tq:WasmToJSObject) does the
// *dynamic* `Is<String> && InSharedSpace` check, so both paths unshare here.
check(wasm.any_string);
check(wasm.ctrl);

// Tier up the export wrappers.
const kBudget = 11;
print(`\nTiering up wrappers (${kBudget} calls each)...`);
for (let i = 0; i < kBudget; i++) { wasm.any_string(); wasm.ctrl(); }

print("\n=== After tier-up (optimized Turboshaft wrapper) ===");
check(wasm.any_string);
check(wasm.ctrl);

// Identity check: after tier-up, any_string() should returns a different
// physical object every call (a fresh local copy).
let a = wasm.any_string();
let b = wasm.any_string();
assertFalse(%IsSameHeapObject(a, b));
