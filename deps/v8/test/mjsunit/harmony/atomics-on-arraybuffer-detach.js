// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test for the buffer becoming detached in surprising places. Test262 covers
// the case where the buffer starts off detached.

function makePoison(arr, val) {
  return { valueOf() { %ArrayBufferDetach(arr.buffer); return val; } };
}

function forEachIntegerTypedArray(cb) {
  for (let C of [Int8Array, Uint8Array, Uint8ClampedArray,
                 Int16Array, Uint16Array,
                 Int32Array, Uint32Array,
                 BigInt64Array, BigUint64Array]) {
    let arr = new C(32);
    let zero, valuePoison;
    if (C === BigInt64Array || C === BigUint64Array) {
      zero = 0n;
      valuePoison = makePoison(arr, 0n);
    } else {
      zero = 0;
      valuePoison = makePoison(arr, 0);
    }
    cb(arr, makePoison(arr, 0), valuePoison, zero);
  }
}

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.add(arr, indexPoison, zero), TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.add(arr, 0, valuePoison), TypeError);
});

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.and(arr, indexPoison, zero), TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.and(arr, 0, valuePoison), TypeError);
});

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.compareExchange(arr, indexPoison, zero, zero),
               TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.compareExchange(arr, 0, valuePoison, zero),
               TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.compareExchange(arr, 0, zero, valuePoison),
               TypeError);
});

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.exchange(arr, indexPoison, zero), TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.exchange(arr, 0, valuePoison), TypeError);
});

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.load(arr, indexPoison), TypeError);
});

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.or(arr, indexPoison, zero), TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.or(arr, 0, valuePoison), TypeError);
});

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.store(arr, indexPoison, 0n), TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.store(arr, 0, valuePoison), TypeError);
});

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.sub(arr, indexPoison, zero), TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.sub(arr, 0, valuePoison), TypeError);
});

forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.xor(arr, indexPoison, zero), TypeError);
});
forEachIntegerTypedArray((arr, indexPoison, valuePoison, zero) => {
  assertThrows(() => Atomics.xor(arr, 0, valuePoison), TypeError);
});

// Atomics.wait always throws on non-SABs.
// Atomics.notify always returns 0 on non-SABs.
