// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

// Private fields should not be able to extend shared objects. This is unlike
// ordinary objects, which can get private fields even if sealed/frozen.

function TestSharedObjectCannotAddPrivateField(sharedObj) {
  function Base() {
    return sharedObj;
  }
  assertThrows(() => {
    class C extends Base {
      #foo = 42
    }
    return new C();
  });
}

TestSharedObjectCannotAddPrivateField(new Atomics.Condition());
TestSharedObjectCannotAddPrivateField(new Atomics.Mutex());
TestSharedObjectCannotAddPrivateField(new (new SharedStructType(['p'])));
TestSharedObjectCannotAddPrivateField(new SharedArray(1));
