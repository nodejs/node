// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

function TestBrandChecks(struct1, struct2, sa, mutex, cv) {
  assertTrue(SharedStructType.isSharedStruct(struct1));
  assertTrue(SharedStructType.isSharedStruct(struct2));
  for (const x of [sa, mutex, cv]) {
    assertFalse(SharedStructType.isSharedStruct(x));
  }

  assertTrue(SharedArray.isSharedArray(sa));
  assertTrue(sa instanceof SharedArray);
  for (const x of [struct1, struct2, mutex, cv]) {
    assertFalse(SharedArray.isSharedArray(x));
  }

  assertTrue(Atomics.Mutex.isMutex(mutex));
  assertTrue(mutex instanceof Atomics.Mutex);
  for (const x of [struct1, struct2, sa, cv]) {
    assertFalse(Atomics.Mutex.isMutex(x));
  }

  assertTrue(Atomics.Condition.isCondition(cv));
  assertTrue(cv instanceof Atomics.Condition);
  for (const x of [struct1, struct2, sa, mutex]) {
    assertFalse(Atomics.Condition.isCondition(x));
  }

  const falsehoods = [42, -0, undefined, null, true, false, 'foo', {}, []];
  for (const x of falsehoods) {
    assertFalse(SharedStructType.isSharedStruct(x));
    assertFalse(SharedArray.isSharedArray(x));
    assertFalse(Atomics.Mutex.isMutex(x));
    assertFalse(Atomics.Condition.isCondition(x));
  }
}

// Test same Realm.
const S1 = new SharedStructType(['field']);
const S2 = new SharedStructType(['field2']);
const struct1 = new S1();
const struct2 = new S2();
const sa = new SharedArray(1);
const mutex = new Atomics.Mutex();
const cv = new Atomics.Condition();
TestBrandChecks(struct1, struct2, sa, mutex, cv);

// Test cross-Realm.
const [otherStruct1, otherStruct2, otherSa, otherMutex, otherCv] =
    Realm.eval(Realm.create(), `const S1 = new SharedStructType(['field']);
const S2 = new SharedStructType(['field2']);
const struct1 = new S1();
const struct2 = new S2();
const sa = new SharedArray(1);
const mutex = new Atomics.Mutex();
const cv = new Atomics.Condition();
[struct1, struct2, sa, mutex, cv]`);
TestBrandChecks(otherStruct1, otherStruct2, otherSa, otherMutex, otherCv);
