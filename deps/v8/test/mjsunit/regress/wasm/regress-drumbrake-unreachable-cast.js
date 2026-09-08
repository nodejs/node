// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Test 1: br_on_cast_fail missing SetUnreachableMode on TypeCheckAlwaysFails branch.
(function testBrOnCastFailUnreachable() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  let $Super = builder.addStruct({fields: [makeField(kWasmI32, true)], supertype: kNoSuperType, final: false});
  let $SubA = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)], supertype: $Super});
  let $SubB = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI64, true)], supertype: $Super});

  let $sig_if = builder.addType(makeSig([wasmRefNullType($SubB)], [wasmRefType($SubB)]));

  builder.addFunction('main', makeSig([], [kWasmI64])).exportFunc()
    .addLocals(wasmRefNullType($Super), 1)
    .addBody([
    // Local 0 = instance of struct $SubA (field 0 = 42, field 1 = 100)
    ...wasmI32Const(42),
    ...wasmI32Const(100),
    kGCPrefix, kExprStructNew, $SubA,
    kExprLocalSet, 0,

    kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(wasmRefNullType($Super)),
      kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(wasmRefNullType($Super)),
        kExprLocalGet, 0,
        // Cast check: (ref $Super) -> (ref null $SubB).
        // TypeCheckAlwaysFails($SubA, $SubB) == true -> emits s2s_Branch to end of block.
        ...wasmBrOnCastFail(1, wasmRefNullType($Super), wasmRefNullType($SubB)),
        // Dead suffix inside block:
        kExprI32Const, 0,
        kExprIf, $sig_if,
          kExprDrop,
          kGCPrefix, kExprStructNewDefault, $SubB,
        kExprElse,
          kExprDrop,
          kGCPrefix, kExprStructNewDefault, $SubB,
        kExprEnd,
        kGCPrefix, kExprRefCast, $SubB,
        kGCPrefix, kExprStructGet, $SubB, 1,
        kExprDrop,
        ...wasmI64Const(0),
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kGCPrefix, kExprStructNewDefault, $Super,
    kExprEnd,

    // Outer block target: local 0 physically contains struct $SubA.
    // Must trap with kTrapIllegalCast.
    kGCPrefix, kExprRefCast, $SubB,
    kGCPrefix, kExprStructGet, $SubB, 1,
  ]);

  const instance = builder.instantiate({});
  assertTraps(kTrapIllegalCast, () => instance.exports.main());
})();

// Test 2: ref.cast / ref.cast_null missing SetUnreachableMode on TypeCheckAlwaysFails branch.
(function testRefCastUnreachable() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  let $Super = builder.addStruct({fields: [makeField(kWasmI32, true)], supertype: kNoSuperType, final: false});
  let $SubA = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)], supertype: $Super});
  let $SubB = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI64, true)], supertype: $Super});

  let $sig_if_cond = builder.addType(makeSig([], []));
  let $sig_if_nested = builder.addType(makeSig([wasmRefNullType($SubB)], [wasmRefNullType($SubB)]));

  builder.addFunction('main', makeSig([], [kWasmI64])).exportFunc()
    .addLocals(wasmRefNullType($Super), 1)
    .addBody([
    // Local 0 = instance of struct $SubA (field 0 = 42, field 1 = 100)
    ...wasmI32Const(42),
    ...wasmI32Const(100),
    kGCPrefix, kExprStructNew, $SubA,
    kExprLocalSet, 0,

    // Outer if: condition 1 (always true)
    ...wasmI32Const(1),
    kExprIf, ...wasmSignedLeb($sig_if_cond),
      // Then arm: empty
    kExprElse,
      // Else arm (runtime skipped):
      // 1. Always-failing ref.cast $SubB on local 0 -> emits s2s_TrapIllegalCast + SetUnreachableMode()
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCast, $SubB,
      kExprDrop,
      // 2. Dead suffix: supply ref.null $SubB and nested if
      kExprRefNull, ...wasmEncodeHeapType(wasmRefNullType($SubB)),
      ...wasmI32Const(0),
      kExprIf, ...wasmSignedLeb($sig_if_nested),
      kExprElse,
      kExprEnd,
      kExprDrop,
    kExprEnd,

    // Outside if: load local 0 (physically contains struct $SubA).
    // Must trap with kTrapIllegalCast.
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, $SubB,
    kGCPrefix, kExprStructGet, $SubB, 1,
  ]);

  const instance = builder.instantiate({});
  assertTraps(kTrapIllegalCast, () => instance.exports.main());
})();

// Test 3: ref.cast null on unrelated nullable type when object is null.
// Exercises TypeCheckAlwaysFails path where null succeeds, verifying RefPush(resulting_value_type) updates stack type.
(function testRefCastNullAlwaysFailsNullSucceeds() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  let $Super = builder.addStruct({fields: [makeField(kWasmI32, true)], supertype: kNoSuperType, final: false});
  let $SubA = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)], supertype: $Super});
  let $SubB = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI64, true)], supertype: $Super});

  builder.addFunction('main', makeSig([], [kWasmI64])).exportFunc()
    .addLocals(wasmRefNullType($SubA), 1) // Local 0 is null (ref null $SubA)
    .addBody([
      // Explicitly push ref.null $SubA
      kExprRefNull, ...wasmEncodeHeapType(wasmRefNullType($SubA)),
      // ref.cast null $SubB on null (ref null $SubA) -> TypeCheckAlwaysFails holds, but null succeeds.
      // Emits s2s_AssertNullTypecheck, pops (ref null $SubA), pushes (ref null $SubB).
      kGCPrefix, kExprRefCastNull, $SubB,
      // struct.get $SubB on the resulting null -> must throw kTrapNullDereference.
      kGCPrefix, kExprStructGet, $SubB, 1,
    ]);

  const instance = builder.instantiate({});
  assertTraps(kTrapNullDereference, () => instance.exports.main());
})();

// Test 4: ref.as_non_null updates stack type to non-nullable (ref $T).
// Verifies RefPush(value_type.AsNonNull()) in kExprRefAsNonNull correctly marks result as non-nullable.
(function testRefAsNonNullUpdatesStackType() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  let $SubA = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)], supertype: kNoSuperType});

  builder.addFunction('main', makeSig([], [kWasmI32])).exportFunc()
    .addLocals(wasmRefNullType($SubA), 1)
    .addBody([
      // Local 0 = struct $SubA instance
      ...wasmI32Const(42),
      ...wasmI32Const(100),
      kGCPrefix, kExprStructNew, $SubA,
      kExprLocalSet, 0,

      kExprLocalGet, 0,
      // ref.as_non_null converts (ref null $SubA) -> (ref $SubA) on stack
      kExprRefAsNonNull,
      // Following ref.cast to non-nullable $SubA:
      // With RefPush(value_type.AsNonNull()), obj_type is non-nullable (ref $SubA).
      // TypeCheckAlwaysSucceeds holds and obj_type is non-nullable -> elides extra s2s_AssertNotNullTypecheck.
      kGCPrefix, kExprRefCast, $SubA,
      kGCPrefix, kExprStructGet, $SubA, 0,
    ]);

  const instance = builder.instantiate({});
  assertEquals(42, instance.exports.main());
})();

// Test 5: Reachable always-failing ref.cast executed directly at runtime.
// Verifies reachable s2s_TrapIllegalCast traps with kTrapIllegalCast without stream corruption.
(function testRefCastReachable() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  let $Super = builder.addStruct({fields: [makeField(kWasmI32, true)], supertype: kNoSuperType, final: false});
  let $SubA = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)], supertype: $Super});
  let $SubB = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI64, true)], supertype: $Super});

  builder.addFunction('main', makeSig([], [kWasmI64])).exportFunc()
    .addLocals(wasmRefNullType($Super), 1)
    .addBody([
      ...wasmI32Const(42),
      ...wasmI32Const(100),
      kGCPrefix, kExprStructNew, $SubA,
      kExprLocalSet, 0,

      kExprLocalGet, 0,
      kGCPrefix, kExprRefCast, $SubB,
      kGCPrefix, kExprStructGet, $SubB, 1,
    ]);

  const instance = builder.instantiate({});
  assertTraps(kTrapIllegalCast, () => instance.exports.main());
})();

// Test 6: br_on_cast missing SetUnreachableMode on TypeCheckAlwaysSucceeds branch.
(function testBrOnCastAlwaysSucceedsUnreachable() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  let $Super = builder.addStruct({fields: [makeField(kWasmI32, true)], supertype: kNoSuperType, final: false});
  let $SubA = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)], supertype: $Super});
  let $SubB = builder.addStruct({fields: [makeField(kWasmI32, true), makeField(kWasmI64, true)], supertype: $Super});

  let $sig_if = builder.addType(makeSig([wasmRefNullType($SubA)], [wasmRefType($SubB)]));

  builder.addFunction('main', makeSig([], [kWasmI64])).exportFunc()
    .addLocals(wasmRefNullType($SubA), 1)
    .addBody([
    // Local 0 = instance of struct $SubA
    ...wasmI32Const(42),
    ...wasmI32Const(100),
    kGCPrefix, kExprStructNew, $SubA,
    kExprLocalSet, 0,

    kExprBlock, kWasmRefNull, ...wasmEncodeHeapType(wasmRefNullType($SubA)),
      kExprLocalGet, 0,
      // Cast check: (ref null $SubA) -> (ref null $SubA).
      // TypeCheckAlwaysSucceeds($SubA, $SubA) == true && null_succeeds == true.
      // Emits s2s_Branch + SetUnreachableMode().
      ...wasmBrOnCast(0, wasmRefNullType($SubA), wasmRefNullType($SubA)),
      // Dead suffix inside block:
      kExprI32Const, 0,
      kExprIf, $sig_if,
        kExprDrop,
        kGCPrefix, kExprStructNewDefault, $SubB,
      kExprElse,
        kExprDrop,
        kGCPrefix, kExprStructNewDefault, $SubB,
      kExprEnd,
      kGCPrefix, kExprRefCast, $SubB,
      kGCPrefix, kExprStructGet, $SubB, 1,
      kExprDrop,
      ...wasmI64Const(0),
      kExprReturn,
    kExprEnd,
    kExprDrop,

    // Outside block: local 0 physically contains struct $SubA.
    // Casting local 0 to $SubB must trap with kTrapIllegalCast.
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, $SubB,
    kGCPrefix, kExprStructGet, $SubB, 1,
  ]);

  const instance = builder.instantiate({});
  assertTraps(kTrapIllegalCast, () => instance.exports.main());
})();
