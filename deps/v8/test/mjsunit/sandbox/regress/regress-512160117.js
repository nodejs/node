// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax --homomorphic-ic

let memoryView = new Sandbox.MemoryView(0, 0x100000000);
let dv = new DataView(memoryView);
function readPtr(addr) {
  return dv.getUint32(addr, true);
}
function getPtr(addr) {
  return readPtr(addr) & ~3;
}

function load(o) {
  return o.x;
}
// We want to have a feedback vector but not be in optimized code.
%EnsureFeedbackVectorForFunction(load);
%NeverOptimizeFunction(load);

for (let i = 0; i < 10; i++) {
  let o = {x: 42};
  o['p' + i] = i;
  load(o);
}

let jsFunctionTypeId = Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE');
let feedbackCellTypeId = Sandbox.getInstanceTypeIdFor('FEEDBACK_CELL_TYPE');
let feedbackVectorTypeId = Sandbox.getInstanceTypeIdFor('FEEDBACK_VECTOR_TYPE');
let homomorphicArrayTypeId =
    Sandbox.getInstanceTypeIdFor('WEAK_HOMOMORPHIC_FIXED_ARRAY_TYPE');

let feedbackCellOffset =
    Sandbox.getFieldOffset(jsFunctionTypeId, 'feedback_cell');
let feedbackVectorOffset = Sandbox.getFieldOffset(feedbackCellTypeId, 'value');
let feedbackVectorLengthOffset =
    Sandbox.getFieldOffset(feedbackVectorTypeId, 'length');
let feedbackVectorDataOffset =
    Sandbox.getFieldOffset(feedbackVectorTypeId, 'data');

let addr = Sandbox.getAddressOf(load);
let feedbackCell = getPtr(addr + feedbackCellOffset);
let feedbackVector = getPtr(feedbackCell + feedbackVectorOffset);

let feedbackVectorLength =
    dv.getUint32(feedbackVector + feedbackVectorLengthOffset, true);
let arrayOffset = -1;

// Find the slot containing WeakHomomorphicFixedArray pointer.
for (let i = 0; i < feedbackVectorLength; i++) {
  let slotOffset = feedbackVectorDataOffset + i * 4;
  let slotVal = readPtr(feedbackVector + slotOffset);
  if ((slotVal & 3) === 1) {  // strong pointer
    let target = slotVal & ~3;
    if (Sandbox.getInstanceTypeIdOfObjectAt(target) ===
        homomorphicArrayTypeId) {
      arrayOffset = target;
      break;
    }
  }
}

assertNotEquals(-1, arrayOffset);

let arrayLengthOffset =
    Sandbox.getFieldOffset(homomorphicArrayTypeId, 'length');
// Corrupt array length to 0 using Sandbox API to trigger underflow
let arrayObj = Sandbox.getObjectAt(arrayOffset);
Sandbox.corruptObjectField(arrayObj, arrayLengthOffset, 0);

// Trigger homomorphic IC lookup
load({x: 42});
