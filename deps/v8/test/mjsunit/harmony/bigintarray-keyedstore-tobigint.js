// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let TypedArrayConstructors = [
  BigUint64Array,
  BigInt64Array,
];

// Test KeyedStore in uninitialized and monomorphic states.
for (let C of TypedArrayConstructors) {
  let keyedSta = function(array) {
    var didRun = false;
    array[0] = {
      valueOf() {
        didRun = true;
        return 42n;
      }
    };

    return { didRun, array };
  };

  for (var i = 0; i < 3; ++i) {
    var { didRun, array } = keyedSta(new C(1));
    assertTrue(didRun);
    assertEquals(array[0], 42n);

    // OOB store
    // FIXME: Throw a TypeError when storing OOB in a TypedArray.
    var { didRun } = keyedSta(new C);
    assertTrue(didRun);
  }
}

// Test KeyedStore in polymorphic and megamorphic states.
do {
  let keyedSta = function(array) {
    var didRun = false;
    array[0] = {
      valueOf() {
        didRun = true;
        return 42n;
      }
    };

    return { didRun, array };
  };

  for (var i = 0; i < 3; ++i) {
    for (var C of TypedArrayConstructors) {
      var { didRun, array } = keyedSta(new C(1));
      assertTrue(didRun);
      assertEquals(array[0], 42n);

      // OOB store
      // FIXME: Throw a TypeError when storing OOB in a TypedArray.
      var { didRun } = keyedSta(new C);
      assertTrue(didRun);
    }
  }
} while (false);
