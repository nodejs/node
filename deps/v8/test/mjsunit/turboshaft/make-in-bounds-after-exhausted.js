// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax


let rab = new ArrayBuffer(3, {maxByteLength: 5});
let ta = new Int8Array(rab);

// Ensure the TypedArray is correctly initialised.
assertEquals(3, ta.length);
assertEquals(0, ta.byteOffset);

ta[0] = 11;
ta[1] = 22;
ta[2] = 33;

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

// Resize buffer to zero.
rab.resize(0);

// TypedArray is now out-of-bounds.
assertEquals(0, ta.length);
assertEquals(0, ta.byteOffset);

// Resize buffer to zero.
rab.resize(0);

// Attempt to fetch the next value. This exhausts the iterator.
%OptimizeFunctionOnNextCall(target);
r = target();
assertOptimized(target);
assertEquals(true, r.done);
assertEquals(undefined, r.value);

// Resize buffer so the typed array is again in-bounds.
rab.resize(5);

// TypedArray is now in-bounds.
assertEquals(5, ta.length);
assertEquals(0, ta.byteOffset);

// Attempt to fetch another value from an already exhausted iterator.
%OptimizeFunctionOnNextCall(target);
r = target();
assertEquals(true, r.done);
assertEquals(undefined, r.value);
assertOptimized(target);
