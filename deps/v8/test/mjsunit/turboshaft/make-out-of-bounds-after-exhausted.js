// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax


let rab = new ArrayBuffer(3, {maxByteLength: 5});
let ta = new Int8Array(rab, 1);

// Ensure the TypedArray is correctly initialised.
assertEquals(ta.length, 2);
assertEquals(ta.byteOffset, 1);

ta[0] = 11;
ta[1] = 22;

let it = ta.values();
let r;

function target() {
  return it.next()
}

// Fetch the first value.
%PrepareFunctionForOptimization(target);

r = target();
assertEquals(false, r.done);
assertEquals(11, r.value);

// Fetch the second value.
r = target();
assertEquals(false, r.done);
assertEquals(22, r.value);

// Iterator is now exhausted.
r = target();
assertEquals(true, r.done);
assertEquals(undefined, r.value);

// Resize buffer to zero.
rab.resize(0);

// TypedArray is now out-of-bounds.
assertEquals(0, ta.length);
assertEquals(0, ta.byteOffset);

// Calling next doesn't throw an error.
%OptimizeFunctionOnNextCall(target);
r = target();
assertEquals(true, r.done);
assertEquals(undefined, r.value);
assertOptimized(target);
