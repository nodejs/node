// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct

'use strict';

let S1 = new SharedStructType(['field']);
let S2 = new SharedStructType(['other_field']);
let S3 = new SharedStructType(['field']);
let other_obj = {'field': 42};

function OtherConstructor(val) {
  return {'val': val + 1};
}

function BuildPrototypeChain(base_obj) {
  let proto = {};
  Object.setPrototypeOf(proto, base_obj);
  for (let i = 0; i < 10; i++) {
    let temp = Object.create(proto);
    proto = temp;
  }

  const handler = {};

  for (let i = 0; i < 10; i++) {
    let temp = new Proxy(proto, handler);
    proto = temp;
  }

  return proto;
}

(function TestSharedStructHasInstance() {
  let s1 = new S1();

  // Returns true for the original constructor.
  assertTrue(s1 instanceof S1);
  // Returns false for a SharedStructType with a different fields.
  assertFalse(s1 instanceof S2);
  // Returns false for a different SharedStructType with the same fields.
  assertFalse(s1 instanceof S3);
  // Returns false for a different object.
  assertFalse(other_obj instanceof S1);
  // Returns false for a different constructor.
  assertFalse(s1 instanceof OtherConstructor);
  // Returns true if the shared struct is in the prototype chain.
  assertTrue(BuildPrototypeChain(s1) instanceof S1);
})();

(function TestSharedArrayHasInstance() {
  let sa = new SharedArray(1);
  let sb = new SharedArray(2);

  // Like regular Arrays,  returns true for SharedArray of any length.
  assertTrue(sa instanceof SharedArray);
  assertTrue(sb instanceof SharedArray);
  // Returns false for a different object.
  assertFalse(other_obj instanceof SharedArray);
  // Returns false for a different constructor.
  assertFalse(sa instanceof OtherConstructor);
  // Returns true if the SharedArray is in the prototype chain.
  assertTrue(BuildPrototypeChain(sa) instanceof SharedArray);
})();

(function TestAtomicsMutexHasInstance() {
  let lock = new Atomics.Mutex();
  // Returns true for the original constructor.
  assertTrue(lock instanceof Atomics.Mutex);
  // Returns false for a different object.
  assertFalse(other_obj instanceof Atomics.Mutex);
  // Returns false for a different constructor.
  assertFalse(lock instanceof OtherConstructor);
  // Returns true if the mutex is in the prototype chain.
  assertTrue(BuildPrototypeChain(lock) instanceof Atomics.Mutex);
})();

(function TestAtomicsConditionHasInstance() {
  let conditon = new Atomics.Condition();
  // Returns true for the original constructor.
  assertTrue(conditon instanceof Atomics.Condition);
  // Returns false for a different object.
  assertFalse(other_obj instanceof Atomics.Condition);
  // Returns false for a different constructor.
  assertFalse(conditon instanceof OtherConstructor);
  // Returns true if the condition is in the prototype chain.
  assertTrue(BuildPrototypeChain(conditon) instanceof Atomics.Condition);
})();

(function TestCallStaticHasInstance() {
  let s1 = new S1();
  // Returns true when calling with the correct constructor as the receiver.
  assertTrue(S1[Symbol.hasInstance].call(S1, s1));
  // Returns true when calling with the correct constructor as the receiver from
  // another shared object's constructor.
  assertTrue(SharedArray[Symbol.hasInstance].call(S1, s1));
  // Returns false when calling with other non-shared constructor as the
  // receiver.
  assertFalse(S1[Symbol.hasInstance].call(OtherConstructor, s1));
  // Returns false when calling with a non-function as receiver.
  assertFalse(S1[Symbol.hasInstance].call({}, s1));
})();

(function TestRecursivePrototype() {
  // Test that we throw when reaching __proto__ recursion limit.
  const v = {}, handler = {};
  const newPrototype = new Proxy(v, handler);
  Object.setPrototypeOf(v, newPrototype);
  assertThrows(()=>{v  instanceof SharedArray});
})();

(function TestInheritHasInstance() {
  const func = () => {};
  const sa = new SharedArray(1);
  Object.setPrototypeOf(func, SharedArray);
  assertFalse(sa instanceof func);
})();
