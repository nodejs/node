// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --no-wasm-gc-structref-as-dataref

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestRefCastNullReturnsNullTypeForNonNullInput() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let consumeRefI31 =
    builder.addFunction(`consumeRefI31`,
                        makeSig([wasmRefType(kWasmI31Ref)], []))
    .addBody([]);

  builder.addFunction(`refCastRemovesNullability`,
                      makeSig([kWasmExternRef], []))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprExternInternalize,
    kExprRefAsNonNull,
    kGCPrefix, kExprRefCastNull, kI31RefCode,
    // ref.cast null pushes a nullable value on the stack even though its input
    // was non-nullable, therefore this call is not spec-compliant.
    kExprCallFunction, consumeRefI31.index,
  ]).exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
               /expected type \(ref i31\), found .* type i31ref/);
})();

(function TestRefCastRemovesNullability() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let i31ToI32 =
    builder.addFunction(`i31ToI32`,
                        makeSig([wasmRefType(kWasmI31Ref)], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprI31GetS
    ]);

  builder.addFunction(`refCastRemovesNullability`,
                      makeSig([kWasmExternRef], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprExternInternalize,
    kGCPrefix, kExprRefCast, kI31RefCode,
    // ref.cast pushes a non-nullable value on the stack even for a nullable
    // input value as the instruction traps on null.
    kExprCallFunction, i31ToI32.index,
  ]).exportFunc();

  let wasm = builder.instantiate().exports;
  assertEquals(42, wasm.refCastRemovesNullability(42));
  assertEquals(-1, wasm.refCastRemovesNullability(-1));
  assertTraps(kTrapIllegalCast, () => wasm.refCastRemovesNullability(null));
})();
