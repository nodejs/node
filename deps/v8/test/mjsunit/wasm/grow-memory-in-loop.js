// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --stress-compaction

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

let initialPages = 1;
let maximumPages = 6;

function generateBuilder() {
  let builder = new WasmModuleBuilder();
  builder.addMemory(initialPages, maximumPages, true);
  builder.addFunction('store', kSig_i_ii)
      .addBody([
        kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem, 0, 0,
        kExprGetLocal, 1
      ])
      .exportFunc();
  return builder;
}

// This test verifies that the effects of growing memory inside a loop
// affect the result of current_memory when the loop is over.
(function TestMemoryGrowInsideLoop() {
  print('TestMemoryGrowInsideLoop ...');
  let deltaPages = 1;
  let builder = generateBuilder();
  builder.addFunction('main', kSig_i_i)
      .addBody([
        // clang-format off
        kExprLoop, kWasmStmt,                   // while
          kExprGetLocal, 0,                     // -
          kExprIf, kWasmStmt,                   // if <param0> != 0
            // Grow memory.
            kExprI32Const, deltaPages,          // -
            kExprMemoryGrow, kMemoryZero,       // grow memory
            kExprDrop,                          // drop the result of grow
            // Decrease loop variable.
            kExprGetLocal, 0,                   // -
            kExprI32Const, 1,                   // -
            kExprI32Sub,                        // -
            kExprSetLocal, 0,                   // decrease <param0>
            kExprBr, 1,                         // continue
          kExprEnd,                             // end if
        kExprEnd,                               // end loop
        // Return the memory size.
        kExprMemorySize, kMemoryZero            // put memory size on stack
        // clang-format on
      ])
      .exportFunc();
  {
    // Avoid the loop branch (not growing memory).
    let instance = builder.instantiate();
    let iterations = 0;
    let expectedPages = initialPages + iterations * deltaPages;
    assertTrue(expectedPages <= maximumPages);
    assertEquals(expectedPages, instance.exports.main(iterations));
  }
  {
    // Enter the loop branch (growing memory).
    let instance = builder.instantiate();
    let iterations = 2;
    let expectedPages = initialPages + iterations * deltaPages;
    assertTrue(expectedPages <= maximumPages);
    assertEquals(expectedPages, instance.exports.main(iterations));
  }
})();

// This test verifies that a loop does not affect the result of current_memory
// when the memory is grown both inside and outside the loop.
(function TestMemoryGrowInsideAndOutsideLoop() {
  print('TestMemoryGrowInsideAndOutsideLoop ...');
  let deltaPagesIn = 1;
  let deltaPagesOut = 2;
  let builder = generateBuilder();
  builder.addFunction('main', kSig_i_i)
      .addBody([
        // clang-format off
        // Grow memory.
        kExprI32Const, deltaPagesOut,           // -
        kExprMemoryGrow, kMemoryZero,           // grow memory
        kExprDrop,                              // drop the result of grow
        kExprLoop, kWasmStmt,                   // while
          kExprGetLocal, 0,                     // -
          kExprIf, kWasmStmt,                   // if <param0> != 0
            // Grow memory.
            kExprI32Const, deltaPagesIn,        // -
            kExprMemoryGrow, kMemoryZero,       // grow memory
            kExprDrop,                          // drop the result of grow
            // Decrease loop variable.
            kExprGetLocal, 0,                   // -
            kExprI32Const, 1,                   // -
            kExprI32Sub,                        // -
            kExprSetLocal, 0,                   // decrease <param0>
            kExprBr, 1,                         // continue
          kExprEnd,                             // end if
        kExprEnd,                               // end loop
        // Return memory size.
        kExprMemorySize, kMemoryZero            // put memory size on stack
        // clang-format on
      ])
      .exportFunc();
  {
    // Avoid the loop branch (grow memory by deltaPagesOut).
    let instance = builder.instantiate();
    let iterations = 0;
    let expectedPages = initialPages + deltaPagesOut;
    assertTrue(expectedPages <= maximumPages);
    assertEquals(expectedPages, instance.exports.main(iterations));
  }
  {
    // Avoid the loop branch (grow memory by deltaPagesOut
    // + iterations * deltaPagesIn).
    let instance = builder.instantiate();
    let iterations = 3;
    let expectedPages =
        initialPages + deltaPagesOut + (iterations * deltaPagesIn);
    assertTrue(expectedPages <= maximumPages);
    assertEquals(expectedPages, instance.exports.main(iterations));
  }
})();

// This test verifies that the effects of writing to memory grown inside a loop
// are retained when the loop is over.
(function TestMemoryGrowAndStoreInsideLoop() {
  print('TestMemoryGrowAndStoreInsideLoop ...');
  let deltaPages = 1;
  let builder = generateBuilder();
  builder.addFunction('main', kSig_i_ii)
      .addBody([
        // clang-format off
        kExprLoop, kWasmStmt,                   // while
          kExprGetLocal, 0,                     // -
          kExprIf, kWasmStmt,                   // if <param0> != 0
            // Grow memory.
            kExprI32Const, deltaPages,          // -
            kExprMemoryGrow, kMemoryZero,       // grow memory
            kExprDrop,                          // drop the result of grow
            // Increase counter in memory.
            kExprGetLocal, 1,                   // put index (for store)
                kExprGetLocal, 1,               // put index (for load)
                kExprI32LoadMem, 0, 0,          // load from grown memory
              kExprI32Const, 1,                 // -
              kExprI32Add,                      // increase counter
            kExprI32StoreMem, 0, 0,             // store counter in memory
            // Decrease loop variable.
            kExprGetLocal, 0,                   // -
            kExprI32Const, 1,                   // -
            kExprI32Sub,                        // -
            kExprSetLocal, 0,                   // decrease <param0>
            kExprBr, 1,                         // continue
          kExprEnd,                             // end if
        kExprEnd,                               // end loop
        // Increase counter in memory.
        kExprGetLocal, 1,                       // -
        kExprI32LoadMem, 0, 0                   // load from grown memory
        // clang-format on
      ])
      .exportFunc();

  let index = 0;
  let initialValue = 1;
  {
    // Avoid the loop (not growing memory).
    let instance = builder.instantiate();
    let iterations = 0;
    let expectedValue = initialValue + iterations;
    instance.exports.store(index, initialValue);
    assertEquals(expectedValue, instance.exports.main(iterations, index));
  }
  {
    // Enter the loop (growing memory + increasing counter in grown memory).
    let instance = builder.instantiate();
    let iterations = 2;
    let expectedValue = initialValue + iterations;
    instance.exports.store(index, initialValue);
    assertEquals(expectedValue, instance.exports.main(iterations, index));
  }
})();

// This test verifies that a loop does not affect the memory when the
// memory is grown both inside and outside the loop.
(function TestMemoryGrowAndStoreInsideAndOutsideLoop() {
  print('TestMemoryGrowAndStoreInsideAndOutsideLoop ...');
  let deltaPagesIn = 1;
  let deltaPagesOut = 2;
  let builder = generateBuilder();
  builder.addFunction('main', kSig_i_ii)
      .addBody([
        // clang-format off
        // Grow memory.
        kExprI32Const, deltaPagesOut,           // -
        kExprMemoryGrow, kMemoryZero,           // grow memory
        kExprDrop,                              // drop the result of grow
        // Increase counter in memory.
        kExprGetLocal, 1,                       // put index (for store)
            kExprGetLocal, 1,                   // put index (for load)
            kExprI32LoadMem, 0, 0,              // load from grown memory
          kExprI32Const, 1,                     // -
          kExprI32Add,                          // increase value on stack
        kExprI32StoreMem, 0, 0,                 // store new value
        // Start loop.
        kExprLoop, kWasmStmt,                   // while
          kExprGetLocal, 0,                     // -
          kExprIf, kWasmStmt,                   // if <param0> != 0
            // Grow memory.
            kExprI32Const, deltaPagesIn,        // -
            kExprMemoryGrow, kMemoryZero,       // grow memory
            kExprDrop,                          // drop the result of grow
            // Increase counter in memory.
            kExprGetLocal, 1,                   // put index (for store)
                kExprGetLocal, 1,               // put index (for load)
                kExprI32LoadMem, 0, 0,          // load from grown memory
              kExprI32Const, 1,                 // -
              kExprI32Add,                      // increase value on stack
            kExprI32StoreMem, 0, 0,             // store new value
            // Decrease loop variable.
            kExprGetLocal, 0,                   // -
            kExprI32Const, 1,                   // -
            kExprI32Sub,                        // -
            kExprSetLocal, 0,                   // decrease <param0>
            kExprBr, 1,                         // continue
          kExprEnd,                             // end if
        kExprEnd,                               // end loop
        // Return counter from memory.
        kExprGetLocal, 1,                       // put index on stack
        kExprI32LoadMem, 0, 0                   // load from grown memory
        // clang-format on
      ])
      .exportFunc();

  let index = 0;
  let initialValue = 1;
  {
    // Avoid the loop (grow memory and increment counter only outside the loop).
    let instance = builder.instantiate();
    let iterations = 0;
    let expectedValue = initialValue + 1;
    instance.exports.store(index, initialValue);
    assertEquals(expectedValue, instance.exports.main(iterations, index));
  }
  {
    // Enter the loop (grow memory and increment counter outside/inside loop).
    let instance = builder.instantiate();
    let iterations = 3;
    let expectedValue = initialValue + iterations + 1;
    instance.exports.store(index, initialValue);
    assertEquals(expectedValue, instance.exports.main(iterations, index));
  }
})();
