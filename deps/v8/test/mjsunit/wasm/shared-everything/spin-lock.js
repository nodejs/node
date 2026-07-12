// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Test cooperative work between two worker threads.
// Each worker thread gets access to a shared struct with an i32 and a threadId.
// Worker 0 may only increase the i32 if it is even.
// Worker 1 may only increase the i32 if it is odd.
// Each worker busy-waits for this condition to be fulfilled, increases the
// value by 1 and writes the result back using a compare-exchange.
// The compare-exchange has to always succeed as the different preconditions
// make the i32 act as a lock here.
(function CooperativeCountingUsingSpinLocks() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType,
                                  false, true);
  builder.addFunction("producer", makeSig([kWasmI32], [wasmRefType(struct)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, struct])
    .exportFunc();
  let instance = builder.instantiate();


  let worker0 = new Worker(workerScript, {type: 'function'});
  let worker1 = new Worker(workerScript, {type: 'function'});

  let struct_obj = instance.exports.producer(0);
  worker0.postMessage({struct: struct_obj, threadId: 0});
  worker1.postMessage({struct: struct_obj, threadId: 1});

  assertEquals({struct: struct_obj, value: 11, count: 6}, worker0.getMessage());
  assertEquals({struct: struct_obj, value: 10, count: 5}, worker1.getMessage());

  function workerScript() {
    function createCountUp() {
      const maxCount = 10;
      let builder = new WasmModuleBuilder();
      let struct = builder.addStruct([makeField(kWasmI32, true)], kNoSuperType,
                                      false, true);
      builder.addFunction("countUp",
          makeSig([wasmRefType(struct), kWasmI32], [kWasmI32, kWasmI32]))
        .addLocals(kWasmI32, 3) // current value, new value, iteration count
        .addBody([
          // Pseudo-code:
          //
          // do {
          //   while (true) {
          //     local[2] = struct.atomic.get<0>(local[0]);
          //     if ((local[2] & 1) == local[1]) break;
          //     atomic.pause();
          //   }
          //
          //   local[3] = local[2] + 1;
          //   if (struct.atomic.cmpxchg<0>(local[0], local[2], local[3])
          //       != local[2]) {
          //     unreachable(); // The cmpxchg failed!
          //   }
          //
          //   ++local[4];
          // } while(local[3] < maxCount);
          // return (local[3], local[4]);
          kExprLoop, kWasmVoid,
            kExprLoop, kWasmVoid,
              kExprLocalGet, 0,
              kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct, 0,
              kExprLocalTee, 2,
              kExprI32Const, 1,
              kExprI32And,
              kExprLocalGet, 1,
              kExprI32Ne,
              kExprIf, kWasmVoid,
                kAtomicPrefix, kExprPause,
                kExprBr, 1,
              kExprEnd,
            kExprEnd,

            kExprLocalGet, 0,
            kExprLocalGet, 2, // expected (the other thread may not change it!)
            kExprLocalGet, 2,
            kExprI32Const, 1,
            kExprI32Add,
            kExprLocalTee, 3,
            kAtomicPrefix, kExprStructAtomicCompareExchange, kAtomicSeqCst,
              struct, 0,
            // The compare exchange must not fail!
            kExprLocalGet, 2,
            kExprI32Ne,
            kExprIf, kWasmVoid,
              kExprUnreachable,
            kExprEnd,

            // Increment the iteration count.
            kExprLocalGet, 4,
            kExprI32Const, 1,
            kExprI32Add,
            kExprLocalSet, 4,

            kExprLocalGet, 3,
            ...wasmI32Const(maxCount),
            kExprI32LtS,
            kExprBrIf, 0,
          kExprEnd,
          kExprLocalGet, 3,
          kExprLocalGet, 4,
        ])
        .exportFunc();
      return builder.instantiate().exports.countUp;
    }

    onmessage = function({data:msg}) {
      d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
      let countUp = createCountUp();
      let [value, count] = countUp(msg.struct, msg.threadId);
      postMessage({struct: msg.struct, value, count});
      return;
    }
  }
})();
