// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --stack-size=100

const kHeapObjectTag = 0x1;
const kJSBoundFunctionType = Sandbox.getInstanceTypeIdFor("JS_BOUND_FUNCTION_TYPE");
const kJSBoundFunctionBoundArgumentsOffset = Sandbox.getFieldOffset(kJSBoundFunctionType, "bound_arguments");
const kFixedArrayLengthOffset = 4;

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const getPtr = (obj) => Sandbox.getAddressOf(obj) + kHeapObjectTag;
const getField = (ptr, offset) => memory.getUint32(ptr + offset - kHeapObjectTag, true);
const setField = (ptr, offset, value) => memory.setUint32(ptr + offset - kHeapObjectTag, value, true);

// Create a bound function.
function target() {}
let b = target.bind(null, 1, 2);

let b_ptr = getPtr(b);
let bound_args = getField(b_ptr, kJSBoundFunctionBoundArgumentsOffset);

// Corrupt bound args length.
setField(bound_args, kFixedArrayLengthOffset, 0xffff0000);

// Trigger stack overflow.
assertThrows(b, RangeError, /Maximum call stack size exceeded/);
