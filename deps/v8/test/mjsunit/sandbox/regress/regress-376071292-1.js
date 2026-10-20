// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --allow-natives-syntax

const kSeqStringType = Sandbox.getInstanceTypeIdFor("SEQ_ONE_BYTE_STRING_TYPE");

// Helper function that spawns a worker thread that corrupts memory in the
// background, constantly flipping the given address between valueA and valueB.
function corruptInBackground(obj_address, field_name, valueA, valueB) {
  function workerTemplate(obj_address, field_name, valueA, valueB) {
    const obj = Sandbox.getObjectAt(obj_address);
    const instance_type = Sandbox.getInstanceTypeIdOf(obj);
    const offset = Sandbox.getFieldOffset(instance_type, field_name);
    const target_address = obj_address + offset;
    const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
    while (true) {
      memory.setUint32(target_address, valueA, true);
      memory.setUint32(target_address, valueB, true);
    }
  }
  const workerCode = new Function(`(${workerTemplate})(${obj_address}, "${
      field_name}", ${valueA}, ${valueB})`);
  return new Worker(workerCode, {type: 'function'});
}

// Create the string object that we'll be corrupting.
let string = Array(0x100).fill("A").join("");
assertEquals(Sandbox.getInstanceTypeIdOf(string), kSeqStringType);
assertEquals(string.length, 0x100);

// Trigger some GCs to move the object to a stable position in memory.
gc();
gc();

// Start the worker thread to corrupt the string in the background.
let obj_address = Sandbox.getAddressOf(string);
let valueA = 0x1;
let valueB = 0x100;
let worker = corruptInBackground(obj_address, 'length', valueA, valueB);

// Perform string Utf8 encoding in the foreground.
for (let i = 0; i < 1000; i++) {
  %StringToCString(string);
}
