// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --sandbox-testing --expose-gc

const kJSArrayType = Sandbox.getInstanceTypeIdFor("JS_ARRAY_TYPE");
const kJSArrayLengthOffset = Sandbox.getFieldOffset(kJSArrayType, "length");

// Helper function that spawns a worker thread that corrupts memory in the
// background, constantly flipping the given address between valueA and valueB.
function corruptInBackground(address, valueA, valueB) {
  function workerTemplate(address, valueA, valueB) {
    let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
    while (true) {
      memory.setUint32(address, valueA, true);
      memory.setUint32(address, valueB, true);
    }
  }
  const workerCode = new Function(
      `(${workerTemplate})(${address}, ${valueA}, ${valueB})`);
  return new Worker(workerCode, {type: 'function'});
}

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

// Create two JSArrays: the targetArray is the one we'll corrupt while the
// templateArray is used to restore the targetArray to its original state
// after every attempt.
let templateArray = [0.0, 1.1, 2.2];
let targetArray = [3.3, 4.4, 5.5];

// Move objects to a stable location.
gc();
gc();

let templateArrayAddress = Sandbox.getAddressOf(templateArray);
let targetArrayAddress = Sandbox.getAddressOf(targetArray);
let arraySize = Sandbox.getSizeOf(targetArray);
assertEquals(arraySize, Sandbox.getSizeOf(templateArray));

// Corrupt the length of the JSArray in a worker thread.
let address = targetArrayAddress + kJSArrayLengthOffset;
let valueA = memory.getUint32(address, true);
let valueB = 0x10000 << 1;  // Should be a Smi
let worker = corruptInBackground(address, valueA, valueB);

// Wait for worker to start working.
while (memory.getUint32(address, true) == valueA) { }

// Try to push an object. We will then first try a fast path, then bail to a
// slower path to transition the array. Even with the corrupted objects, we
// must not get confused about how many remaining arguments we need to process,
// so this should either succeed or crash safely.
for (let i = 0; i < 1000; i++) {
  // Restore the target array based on the template array before every attempt.
  for (let fieldOffset = 0; fieldOffset < arraySize; fieldOffset += 4) {
    let fieldValue = memory.getUint32(templateArrayAddress + fieldOffset, true);
    memory.setUint32(targetArrayAddress + fieldOffset, fieldValue, true);
  }

  // Push some double values first to increase the race window.
  targetArray.push(1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, {});
}
