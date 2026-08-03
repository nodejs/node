// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestTableNumericOps() {
  print(arguments.callee.name);
  let kTableNum = 10;
  for (let table_index of [0, 7, 9]) {
    let builder = new WasmModuleBuilder();
    let kTableSize = 5;

    for (let i = 0; i < kTableNum; i++) {
      builder.addTable(kWasmFuncRef, kTableSize);
    }

    let elements = [];

    let sig_i_v = builder.addType(kSig_i_v);

    for (let i = 0; i < kTableSize; i++) {
      builder.addFunction("f" + i, sig_i_v).addBody([kExprI32Const, i]);
      elements.push(i);
    }

    let passive = builder.addPassiveElementSegment(elements);

    let sig_i_i = builder.addType(kSig_i_i);

    builder.addFunction("call", sig_i_i)
      .addBody([kExprLocalGet, 0, kExprCallIndirect, sig_i_v, table_index])
      .exportFunc();
    builder.addFunction("table_init", kSig_v_iii)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
                kNumericPrefix, kExprTableInit, passive, table_index])
      .exportFunc();
    builder.addFunction("drop", kSig_v_v)
      .addBody([kNumericPrefix, kExprElemDrop, passive])
      .exportFunc();

    let wasm = builder.instantiate().exports;

    // An out-of-bounds trapping initialization should not have an effect on the
    // table.
    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(3, 0, 3));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(0));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(1));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(2));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(3));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(4));

    assertTraps(kTrapElementSegmentOutOfBounds, () => wasm.table_init(0, 3, 3));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(0));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(1));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(2));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(3));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(4));

    // 0-count is still oob if target is invalid.
    assertTraps(kTrapTableOutOfBounds,
                () => wasm.table_init(kTableSize + 1, 0, 0));
    assertTraps(kTrapElementSegmentOutOfBounds,
                () => wasm.table_init(0, kTableSize + 1, 0));

    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(0, 0, 6));
    assertTraps(kTrapElementSegmentOutOfBounds, () => wasm.table_init(0, 1, 5));
    assertTraps(kTrapElementSegmentOutOfBounds, () => wasm.table_init(0, 2, 4));
    assertTraps(kTrapElementSegmentOutOfBounds, () => wasm.table_init(0, 3, 3));
    assertTraps(kTrapElementSegmentOutOfBounds, () => wasm.table_init(0, 4, 2));
    assertTraps(kTrapElementSegmentOutOfBounds, () => wasm.table_init(0, 5, 1));
    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(0, 0, 6));
    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(1, 0, 5));
    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(2, 0, 4));
    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(3, 0, 3));
    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(4, 0, 2));
    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(5, 0, 1));
    assertTraps(kTrapTableOutOfBounds, () => wasm.table_init(10, 0, 1));
    assertTraps(kTrapElementSegmentOutOfBounds,
                () => wasm.table_init(0, 10, 1));

    // Initializing 0 elements is ok, even at the end of the table/segment.
    wasm.table_init(0, 0, 0);
    wasm.table_init(kTableSize, 0, 0);
    wasm.table_init(0, kTableSize, 0);

    wasm.table_init(0, 0, 1);
    assertEquals(0, wasm.call(0));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(1));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(2));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(3));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(4));

    wasm.table_init(0, 0, 2);
    assertEquals(0, wasm.call(0));
    assertEquals(1, wasm.call(1));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(2));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(3));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(4));

    wasm.table_init(0, 0, 3);
    assertEquals(0, wasm.call(0));
    assertEquals(1, wasm.call(1));
    assertEquals(2, wasm.call(2));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(3));
    assertTraps(kTrapFuncSigMismatch, () => wasm.call(4));

    wasm.table_init(3, 0, 2);
    assertEquals(0, wasm.call(0));
    assertEquals(1, wasm.call(1));
    assertEquals(2, wasm.call(2));
    assertEquals(0, wasm.call(3));
    assertEquals(1, wasm.call(4));

    wasm.table_init(3, 1, 2);
    assertEquals(0, wasm.call(0));
    assertEquals(1, wasm.call(1));
    assertEquals(2, wasm.call(2));
    assertEquals(1, wasm.call(3));
    assertEquals(2, wasm.call(4));

    wasm.table_init(3, 2, 2);
    assertEquals(0, wasm.call(0));
    assertEquals(1, wasm.call(1));
    assertEquals(2, wasm.call(2));
    assertEquals(2, wasm.call(3));
    assertEquals(3, wasm.call(4));

    wasm.table_init(3, 3, 2);
    assertEquals(0, wasm.call(0));
    assertEquals(1, wasm.call(1));
    assertEquals(2, wasm.call(2));
    assertEquals(3, wasm.call(3));
    assertEquals(4, wasm.call(4));

    // Now drop the passive segment twice. This should work.
    wasm.drop();
    wasm.drop();

    // Subsequent accesses should trap for size > 0.
    wasm.table_init(0, 0, 0);
    assertTraps(kTrapElementSegmentOutOfBounds, () => wasm.table_init(0, 1, 0));
  }
})();
