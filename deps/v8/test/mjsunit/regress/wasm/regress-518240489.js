// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

// Tests that ArrayAtomicCompareExchange in Liftoff correctly passes the base
// address of the object to EmitWriteBarrier on ia32.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

const arrT = builder.addArray(wasmRefNullType(kWasmEqRef), {mutable: true});
// A tiny struct used as the young-gen "new value" for the cmpxchg.
const structT = builder.addStruct([makeField(kWasmI32, true)]);

// Create a large eqref array filled with i31(2) (raw tagged word 0x4).
// 200,000 * 4 bytes = ~781KB body -> allocated in Large Object space and
// spans three 256KB-aligned regions past the chunk header.
builder.addFunction('makeArray', makeSig([], [kWasmAnyRef]))
  .addBody([
    // init value: i31(2).
    ...wasmI32Const(2), kGCPrefix, kExprRefI31,
    ...wasmI32Const(200000),  // length
    kGCPrefix, kExprArrayNew, arrT,
  ])
  .exportFunc();

// Perform array.atomic.rmw.cmpxchg on a (mut eqref) slot:
//   expected = i31(2), new = freshly-allocated struct.
builder.addFunction(
    'trigger',
    makeSig([wasmRefNullType(kWasmAnyRef), kWasmI32],
            [wasmRefNullType(kWasmEqRef)]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCast, arrT,
    kExprLocalGet, 1,                            // index
    ...wasmI32Const(2), kGCPrefix, kExprRefI31,  // expected = i31(2)
    kGCPrefix, kExprStructNewDefault, structT,   // new = young struct
    kAtomicPrefix, kExprArrayAtomicCompareExchange, kAtomicSeqCst, arrT,
  ])
  .exportFunc();

const wasm = builder.instantiate().exports;
const arr = wasm.makeArray();

// Index 100,000 -> mem_location is ~400,000 bytes past the array start, well
// inside the 2nd 256KB-aligned region of the LO chunk. Test that despite that,
// EmitWriteBarrier lands in the real chunk header.
wasm.trigger(arr, 100000);
