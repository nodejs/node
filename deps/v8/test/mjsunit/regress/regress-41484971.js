// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let log = [];

class A {
  constructor() {
    this.prop = 1;
  }
}

function trigger() {
    class B extends A {
        set prop(x) { log.push(x); }

        prop = 12341234;
    }

    return new B();
}

let n = 20;
for (let j = 0; j < n; j++) {
  trigger();
}
let expected = Array(n);
expected.fill(1);
assertEquals(expected, log);
