// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --jitless

load('test/mjsunit/wasm/wasm-module-builder.js');

(function TestCastBranchParameters() {
  const builder = new WasmModuleBuilder();
  const struct = builder.addStruct([makeField(kWasmI32, false)]);
  const otherStruct = builder.addStruct([makeField(kWasmI64, false)]);
  const nullable = wasmRefNullType(struct);
  const nonNullable = wasmRefType(struct);
  const eqref = wasmRefNullType(kWasmEqRef);

  builder.addFunction('make', makeSig([kWasmI32], [nonNullable]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, struct,
      ])
      .exportFunc();

  builder.addFunction('make_other', makeSig([], [wasmRefType(otherStruct)]))
      .addBody([kGCPrefix, kExprStructNewDefault, otherStruct])
      .exportFunc();

  function addLoop(name, castFails, source = nullable, target = nonNullable,
                   operandType = source) {
    const reference = castFails ? source : target;
    const types = [kWasmI32, reference];
    const signature = builder.addType(makeSig(types, types));
    const branch = castFails ? wasmBrOnCastFail : wasmBrOnCast;
    const unconditional = !castFails &&
        (source === nonNullable || target === nullable);
    builder.addFunction(name, makeSig([reference, operandType], types))
        .addLocals(kWasmI32, 1)
        .addBody([
          ...wasmI32Const(11),
          kExprLocalGet, 0,
          kExprLoop, ...wasmSignedLeb(signature),
            // Keep the original parameters live below distinct branch values.
            // A taken loop branch retries once with the new parameters.
            kExprLocalGet, 2,
            kExprI32Eqz,
            kExprIf, kWasmVoid,
              ...wasmI32Const(1),
              kExprLocalSet, 2,
              ...wasmI32Const(21),
              kExprLocalGet, 1,
              ...branch(1, source, target),
              // A proven unconditional cast leaves this suffix unreachable.
              ...(unconditional ? [
                kExprBlock, kWasmI64,
                  ...wasmI64Const(1),
                  ...wasmI64Const(2),
                  kExprI64Add,
                kExprEnd,
                kExprDrop,
              ] : []),
              kExprDrop, kExprDrop,
            kExprEnd,
          kExprEnd,
        ])
        .exportFunc();
  }

  addLoop('cast', false);
  addLoop('cast_fail', true);
  // The operand local itself is eqref, so these require dynamic type checks.
  addLoop('dynamic_cast', false, eqref);
  addLoop('dynamic_cast_fail', true, eqref);
  // An unrelated narrow operand under a valid eqref source always fails.
  addLoop('cast_fail_always_fails', true, eqref, nonNullable,
          wasmRefType(otherStruct));
  addLoop('cast_nullable_target', false, nullable, nullable);
  addLoop('cast_fail_nullable_target', true, nullable, nullable);
  addLoop('cast_non_nullable_operand', false, nonNullable);
  addLoop('cast_fail_non_nullable_operand', true, nonNullable);

  function addNullLoop(branchOnNonNull) {
    const types = [kWasmI32, nonNullable];
    const signature = builder.addType(makeSig(types, types));
    builder.addFunction(branchOnNonNull ? 'non_null' : 'null',
                        makeSig([nonNullable, nullable, nonNullable], types))
        .addLocals(kWasmI32, 1)
        .addBody([
          ...wasmI32Const(11),
          kExprLocalGet, 0,
          kExprLoop, ...wasmSignedLeb(signature),
            kExprLocalGet, 3,
            kExprI32Eqz,
            kExprIf, kWasmVoid,
              ...wasmI32Const(1),
              kExprLocalSet, 3,
              ...wasmI32Const(21),
              // br_on_null consumes a separate condition, not a label argument.
              ...(branchOnNonNull ? [] : [kExprLocalGet, 2]),
              kExprLocalGet, 1,
              branchOnNonNull ? kExprBrOnNonNull : kExprBrOnNull, 1,
              // br_on_non_null already drops null on fallthrough.
              ...(branchOnNonNull ? [] : [kExprDrop, kExprDrop]),
              kExprDrop,
            kExprEnd,
          kExprEnd,
        ])
        .exportFunc();
  }
  addNullLoop(false);
  addNullLoop(true);

  builder.addFunction('cast_fallthrough_nullability',
                      makeSig([nullable], [nonNullable]))
      .addBody([
        kExprBlock, kWasmRef, ...wasmSignedLeb(struct),
          kExprLocalGet, 0,
          ...wasmBrOnCast(0, nullable, nonNullable),
          kGCPrefix, kExprRefCast, ...wasmSignedLeb(struct),
        kExprEnd,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const original = instance.exports.make(11);
  const different = instance.exports.make(99);
  const other = instance.exports.make_other();
  assertTraps(kTrapIllegalCast,
              () => instance.exports.cast_fallthrough_nullability(null));
  assertSame(different,
             instance.exports.cast_fallthrough_nullability(different));

  let result = instance.exports.cast(original, null);
  assertEquals(11, result[0]);
  assertSame(original, result[1]);

  result = instance.exports.cast(original, different);
  assertEquals(21, result[0]);
  assertSame(different, result[1]);

  result = instance.exports.cast_fail(original, different);
  assertEquals(11, result[0]);
  assertSame(original, result[1]);

  result = instance.exports.cast_fail(original, null);
  assertEquals(21, result[0]);
  assertSame(null, result[1]);

  function check(name, operand, taken, candidate = operand) {
    const result = instance.exports[name](original, operand, candidate);
    assertEquals(taken ? 21 : 11, result[0], name);
    assertSame(taken ? candidate : original, result[1], name);
  }

  check('dynamic_cast', different, true);
  check('dynamic_cast', other, false);
  check('dynamic_cast', null, false);
  check('dynamic_cast_fail', different, false);
  check('dynamic_cast_fail', other, true);
  check('dynamic_cast_fail', null, true);
  check('cast_fail_always_fails', other, true);
  // Only br_on_cast branches in these always-successful configurations.
  check('cast_nullable_target', null, true);
  check('cast_nullable_target', different, true);
  check('cast_fail_nullable_target', null, false);
  check('cast_fail_nullable_target', different, false);
  check('cast_non_nullable_operand', different, true);
  check('cast_fail_non_nullable_operand', different, false);
  check('null', null, true, different);
  check('null', different, false, different);
  check('non_null', different, true);
  check('non_null', null, false, different);
})();

(function TestCastBranchFunctionTable() {
  const builder = new WasmModuleBuilder();
  const signature = builder.addType(kSig_i_v);
  const nullable = wasmRefNullType(signature);
  const nonNullable = wasmRefType(signature);
  const loopSignature =
      builder.addType(makeSig([nonNullable], [nonNullable]));
  const original = builder.addFunction('original', signature)
      .addBody([...wasmI32Const(42)]);
  const different = builder.addFunction('different', signature)
      .addBody([...wasmI32Const(99)]);
  const table = builder.addTable(nonNullable, 2, 3,
                                 [kExprRefFunc, different.index])
      .exportAs('table');
  builder.addDeclarativeElementSegment([original.index]);

  function preservedReference() {
    return [
      kExprRefFunc, original.index,
      kExprLoop, ...wasmSignedLeb(loopSignature),
        // The null cast operand must not overwrite the live loop parameter.
        kExprRefNull, ...wasmSignedLeb(signature),
        ...wasmBrOnCast(0, nullable, nonNullable),
        kExprDrop,
      kExprEnd,
    ];
  }

  builder.addFunction('grow', kSig_i_v)
      .addBody([
        ...preservedReference(),
        ...wasmI32Const(1),
        kNumericPrefix, kExprTableGrow, table.index,
      ])
      .exportFunc();
  builder.addFunction('set', kSig_v_v)
      .addBody([
        ...wasmI32Const(0),
        ...preservedReference(),
        kExprTableSet, table.index,
      ])
      .exportFunc();
  builder.addFunction('fill', kSig_v_v)
      .addBody([
        ...wasmI32Const(0),
        ...preservedReference(),
        ...wasmI32Const(2),
        kNumericPrefix, kExprTableFill, table.index,
      ])
      .exportFunc();
  builder.addFunction('call', kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprCallIndirect, signature, table.index,
      ])
      .exportFunc();

  const module = builder.toModule();
  const grow = new WebAssembly.Instance(module).exports;
  assertEquals(2, grow.grow());
  assertEquals(3, grow.table.length);
  assertEquals(42, grow.call(2));

  const set = new WebAssembly.Instance(module).exports;
  set.set();
  assertEquals(42, set.call(0));

  const fill = new WebAssembly.Instance(module).exports;
  fill.fill();
  assertEquals(42, fill.call(0));
  assertEquals(42, fill.call(1));
})();
