// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc-experiments --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// This is benchmark to investigate at which point it is more efficient to call
// a memcpy-based builtin for array.copy, rather than copying
// element-by-element.
// How to run:
// - Set {iterations} to a high number to get better measurements
// - Change the value of {length} to find point at which the builtin becomes
//   faster.
// - Change {array_type} if you want to test different types.
// Right now, the limit is found to be in the 25-30 range.
// TODO(7748): Measure again if we implement array.copy with a fast C call.
(function ArrayCopyBenchmark() {

  let array_length = 27;
  let iterations = 1;

  var builder = new WasmModuleBuilder();
  let struct_index = builder.addStruct([makeField(kWasmI32, true),
                                        makeField(kWasmI8, false)]);
  let array_type = kWasmI32;  // Also try kWasmI64, wasmOptRefType(struct_index)
  var array_index = builder.addArray(array_type, true);
  var from = builder.addGlobal(wasmOptRefType(array_index), true);
  var to = builder.addGlobal(wasmOptRefType(array_index), true);

  builder.addFunction("init", kSig_v_v)
    .addBody([
      ...wasmI32Const(array_length),
      kGCPrefix, kExprRttCanon, array_index,
      kGCPrefix, kExprArrayNewDefault, array_index,
      kExprGlobalSet, from.index,
      ...wasmI32Const(array_length),
      kGCPrefix, kExprRttCanon, array_index,
      kGCPrefix, kExprArrayNewDefault, array_index,
      kExprGlobalSet, to.index
    ])
    .exportFunc();

  builder.addFunction("array_copy", kSig_v_v)
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprLoop, kWasmVoid,
        kExprGlobalGet, to.index,
        ...wasmI32Const(0),
        kExprGlobalGet, from.index,
        ...wasmI32Const(0),
        ...wasmI32Const(array_length),
        kGCPrefix, kExprArrayCopy, array_index, array_index,
        // Outer loop: run everything {iterations} times.
        kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add, kExprLocalSet, 0,
        kExprLocalGet, 0, ...wasmI32Const(iterations), kExprI32LtS,
        kExprBrIf, 0,
      kExprEnd])
    .exportFunc();

  builder.addFunction("loop_copy", kSig_v_v)
    .addLocals(kWasmI32, 2)
    .addBody([
      kExprLoop, kWasmVoid,
        ...wasmI32Const(0),
        kExprLocalSet, 0,
        kExprGlobalGet, from.index, kExprRefAsNonNull,
        kExprGlobalGet, to.index, kExprRefAsNonNull,
        kExprLet, kWasmVoid, 1, 2, kWasmRef, array_index,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 1,  // array
          kExprLocalGet, 2,  // index
          // value
          kExprLocalGet, 0, kExprLocalGet, 2,
          kGCPrefix, kExprArrayGet, array_index,
          // array.set
          kGCPrefix, kExprArraySet, array_index,
          // index++
          kExprLocalGet, 2, kExprI32Const, 1, kExprI32Add, kExprLocalSet, 2,
          // if (index < array_length) goto loop;
          kExprLocalGet, 2, ...wasmI32Const(array_length), kExprI32LtU,
          kExprBrIf, 0,
        kExprEnd,
        kExprEnd,
      // Outer loop: run everything {iterations} times.
      kExprLocalGet, 1, kExprI32Const, 1, kExprI32Add, kExprLocalSet, 1,
      kExprLocalGet, 1, ...wasmI32Const(iterations), kExprI32LtS,
      kExprBrIf, 0,
      kExprEnd])
    .exportFunc();

  var instance = builder.instantiate({});

  instance.exports.init();
  print("Array length: " + array_length + ", #iterations: " + iterations);
  {
    let before = Date.now();
    instance.exports.array_copy();
    let after = Date.now();
    print("array.copy: " + (after - before) + "ms");
  }
  {
    let before = Date.now();
    instance.exports.loop_copy();
    let after = Date.now();
    print("loop copy: " + (after - before) + "ms");
  }
})();
