// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

let mutex = new Atomics.Mutex;
let locked_count = 0;

assertEquals(42, Atomics.Mutex.lock(mutex, () => {
  locked_count++; return 42;
}));
assertEquals(locked_count, 1);

// tryLock returns true when successful.
assertTrue(Atomics.Mutex.tryLock(mutex, () => { locked_count++; }));
assertEquals(locked_count, 2);

// Recursively locking throws.
Atomics.Mutex.lock(mutex, () => {
  locked_count++;
  assertThrows(() => {
    Atomics.Mutex.lock(mutex, () => { throw "unreachable"; });
  }, Error);
});
assertEquals(locked_count, 3);

// Recursive tryLock'ing returns false.
Atomics.Mutex.lock(mutex, () => {
  locked_count++;
  assertFalse(Atomics.Mutex.tryLock(mutex, () => { throw "unreachable"; }));
});
assertEquals(locked_count, 4);

// Throwing in the callback should unlock the mutex.
assertThrowsEquals(() => {
  Atomics.Mutex.lock(mutex, () => { throw 42; });
}, 42);
Atomics.Mutex.lock(mutex, () => { locked_count++; });
assertEquals(locked_count, 5);

assertThrowsEquals(() => {
  Atomics.Mutex.tryLock(mutex, () => { throw 42; });
}, 42);
Atomics.Mutex.tryLock(mutex, () => { locked_count++; });
assertEquals(locked_count, 6);
