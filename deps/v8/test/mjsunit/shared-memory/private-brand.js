// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

// Private brands from private methods should not be able to extend shared
// objects. This is unlike ordinary objects, which can get private fields even
// if sealed/frozen.

function TestSharedObjectCannotAddPrivateBrand(sharedObj) {
  function Base() {
    return sharedObj;
  }
  assertThrows(() => {
    class C extends Base {
      #o() {}
    }
    return new C();
  });
}

TestSharedObjectCannotAddPrivateBrand(new Atomics.Condition());
TestSharedObjectCannotAddPrivateBrand(new Atomics.Mutex());
TestSharedObjectCannotAddPrivateBrand(new (new SharedStructType(['p'])));
TestSharedObjectCannotAddPrivateBrand(new SharedArray(1));
