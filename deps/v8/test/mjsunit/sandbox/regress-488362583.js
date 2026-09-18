// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation
// Flags: --allow-natives-syntax --sandbox-testing

const kHeapObjectTag = 0x1;
const kSmiTagSize = 0x1;
const kJSFunctionType = Sandbox.getInstanceTypeIdFor("JS_FUNCTION_TYPE");
const kJSFunctionFeedbackCellOffset = Sandbox.getFieldOffset(kJSFunctionType, "feedback_cell");
const kFeedbackCellType = Sandbox.getInstanceTypeIdFor("FEEDBACK_CELL_TYPE");
const kFeedbackCellValueOffset = Sandbox.getFieldOffset(kFeedbackCellType, "value");

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const getPtr = (obj) => Sandbox.getAddressOf(obj) + kHeapObjectTag;
const getObj = (ptr) => Sandbox.getObjectAt(ptr);
const getField = (ptr, offset) => memory.getUint32(ptr + offset - kHeapObjectTag, true);
const setField = (ptr, offset, value) => memory.setUint32(ptr + offset - kHeapObjectTag, value, true);
const getInstanceType = (ptr) => Sandbox.getInstanceTypeOfObjectAt(ptr);

function f(o,i,v) {
  o[i] = v;
}

%PrepareFunctionForOptimization(f);

let o = Array(20);
o.__proto__ = null;

for (let i = 0; i < 20; i++) {
  f(o, i, 42);
}

// Allocate the array before manipulating the memory.
let builtins = Sandbox.getBuiltinNames();

// Corrupt store handler in the veedback vector.
{
  let fp = getPtr(f);
  let fc = getField(fp, kJSFunctionFeedbackCellOffset);
  let fv = getField(fc, kFeedbackCellValueOffset);

  // Read handler from slot #0.
  let fv_handler_offset = 4 * (7 + 1);
  let store_handler = getField(fv, fv_handler_offset);
  assertEquals(getInstanceType(store_handler), "CODE_TYPE")

  // Replace StoreFastElementIC_InBounds handler with an incompatible one
  // forming a non-canonial address having an ASAN shadow memory in
  // canonical address range.
  let builtin = builtins.indexOf("ElementsTransitionAndStore_InBounds");
  assertNotEquals(builtin, -1);
  setField(fv, fv_handler_offset, Sandbox.getBuiltinCode(builtin));
}

// Crash.
f(o, 3, 42);

assertUnreachable();
