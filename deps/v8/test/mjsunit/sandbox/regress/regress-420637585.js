// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --allow-natives-syntax

const kJSArrayType = Sandbox.getInstanceTypeIdFor("JS_ARRAY_TYPE");
const kJSArrayLengthOffset = Sandbox.getFieldOffset(kJSArrayType, "length");
const GB = 1024 * 1024 * 1024;
const TB = 1024 * GB;
const kSandboxSize = 1 * TB;

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

let arr = [0.0, 1.1, 2.2, 3.3];

// Move array to a stable position in memory.
gc();
gc();

function put(i, v) {
  arr[i] = v;
}

put(0, 1.1);
%PrepareFunctionForOptimization(put);
put(1, 2.2);
%OptimizeFunctionOnNextCall(put);
put(2, 3.3);

let s = Sandbox.getAddressOf(arr);
memory.setUint32(s + kJSArrayLengthOffset, -1, true);

// Access somewhere past the end of the sandbox.
let offset = kSandboxSize + Math.random() * TB
let index = Math.floor(offset / 8);
put(index, 1.1);
