// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// The JIT tiers report a table trap for table.init operands that do not fit
// in a Smi, also if the element segment offset is the operand that is out of
// bounds. The interpreter must report the same trap reason. The operands used
// below (>= 2^31) do not fit in a Smi on any supported configuration.

const builder = new WasmModuleBuilder();
const $f = builder.addFunction('f', kSig_v_v).addBody([]);
const $table = builder.addTable(kWasmFuncRef, 1, 1).index;
const $seg =
    builder.addPassiveElementSegment([[kExprRefFunc, $f.index]], kWasmFuncRef);

function addTableInit(name, dst, src, n) {
  builder.addFunction(name, kSig_v_v)
      .addBody([
        ...wasmI32Const(dst), ...wasmI32Const(src), ...wasmI32Const(n),
        kNumericPrefix, kExprTableInit, $seg, $table,
      ])
      .exportFunc();
}
addTableInit('src_oob', 0, 1, 1);
addTableInit('src_oob_big', 0, 0x80000000, 1);
addTableInit('src_oob_huge', 0, 0xfffffff0, 1);
addTableInit('dst_oob', 1, 0, 1);
addTableInit('size_oob_big', 0, 0, 0x80000000);

const instance = builder.instantiate();
assertTraps(kTrapElementSegmentOutOfBounds, instance.exports.src_oob);
assertTraps(kTrapTableOutOfBounds, instance.exports.src_oob_big);
assertTraps(kTrapTableOutOfBounds, instance.exports.src_oob_huge);
assertTraps(kTrapTableOutOfBounds, instance.exports.dst_oob);
assertTraps(kTrapTableOutOfBounds, instance.exports.size_oob_big);
