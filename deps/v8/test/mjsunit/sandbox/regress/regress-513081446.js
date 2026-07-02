// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

let memoryView = new Sandbox.MemoryView(0, 0x100000000);
let dv = new DataView(memoryView);
function readPtr(addr) {
  return dv.getUint32(addr, true);
}

// 1. Define our target function containing the raced nested-boilerplate
// layout.
function target() {
  return {
    a: 1,
    b: 2,
    nested: [100, 200]
  };
}
%EnsureFeedbackVectorForFunction(target);
%NeverOptimizeFunction(target);
target();

// 2. Fetch the constant pool using natives syntax.
let pool = %GetBytecode(target).constant_pool;
if (!pool || pool.length === 0) {
  throw new Error("Empty constant pool!");
}

// 3. The ObjectBoilerplateDescription is the first and only object in
// the pool.
let boilerplate = pool[0];
if (!boilerplate) {
  throw new Error("Boilerplate element is null!");
}

let boilerplateAddr = Sandbox.getAddressOf(boilerplate);

// 4. Map target memory slots under 4GB compressed sandbox base.
// Offset 28 -> Property 1 (b: 2) value slot
// Offset 36 -> Property 2 (nested) value slot
//              (compressed ArrayBoilerplate pointer)
let targetSlot = boilerplateAddr + 28;
let boilerplateArrayVal = readPtr(boilerplateAddr + 36);

// 5. Spawn background Web Worker to concurrently race and mutate the value.
// Uses a finite high-count loop (50k iterations) to safely exit without
// locking d8's non-preemptive thread scheduler.
const workerScript = `
  onmessage = function(e) {
    let memoryView = new Sandbox.MemoryView(0, 0x100000000);
    let dv = new DataView(memoryView);
    let targetSlot = e.data.targetSlot;
    let boilerplateArrayVal = e.data.boilerplateArrayVal;

    // Mutate the target slot concurrently from Smi(0) to ArrayBoilerplate
    // pointer (compressed tag)!
    for (let i = 0; i < 50000; i++) {
      dv.setUint32(targetSlot, boilerplateArrayVal, true);
      dv.setUint32(targetSlot, 0, true);
    }
  };
`;

const w = new Worker(workerScript, {type: 'string'});
w.postMessage({ targetSlot, boilerplateArrayVal });

// 6. Main execution loop. Attempt to trigger object-literal instantiations.
for (let i = 0; i < 10000; i++) {
  target();
}

w.terminate();
console.log("PASS");
