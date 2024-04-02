// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc
// Stress-testing this test is very slow and doesn't provide useful coverage.
// Flags: --nostress-opt --noalways-opt

const f = eval(`(function f(i) {
  if (i == 0) {
    class Derived extends Object {
      constructor() {
        super();
        ${"this.a=1;".repeat(0x3fffe-8)}
      }
    }
    return Derived;
  }

  class DerivedN extends f(i-1) {
    constructor() {
      super();
      ${"this.a=1;".repeat(0x40000-8)}
    }
  }

  return DerivedN;
})`);

let a = new (f(0x7ff))();
a.a = 1;
gc();
assertEquals(1, a.a);
