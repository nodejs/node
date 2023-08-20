// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.prototype[1] = 153;

(function TestSloppyStoreToReadOnlyProperty() {
  function foo(prototype_frozen) {
    let ar = [];
    for (let i = 0; i < 3; i++) {
      ar[i] = 42;

      if (prototype_frozen) {
        if (i == 1) {
          // Attempt to overwrite read-only element should not change
          // array length.
          assertEquals(1, ar.length);
        } else {
          assertEquals(i + 1, ar.length);
        }
      }
    }
    return ar;
  }

  // Warm-up store IC.
  assertEquals([42,42,42], foo(false));
  assertEquals([42,42,42], foo(false));
  assertEquals([42,42,42], foo(false));
  assertEquals([42,42,42], foo(false));
  Object.freeze(Object.prototype);
  // Ensure IC was properly invalidated.
  assertEquals([42,153,42], foo(true));
})();
