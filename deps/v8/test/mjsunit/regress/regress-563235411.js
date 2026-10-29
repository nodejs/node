// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

class Base {
  constructor(o) {
    return o;
  }
}

class Derived extends Base {
  #x = 42;
  #m() {
    return this.#x;
  }
  static getX(o) {
    return o.#x;
  }
  static setX(o, v) {
    o.#x = v;
  }
  static callM(o) {
    return o.#m();
  }
}

for (let i = 0; i < 20; i++) {
  const allowed = d8.test.createAccessCheckedObject(true);
  new Derived(allowed);
  assertEquals(42, Derived.getX(allowed));
  assertEquals(42, Derived.callM(allowed));

  for (let j = 0; j < 5; j++) {
    Derived.setX(allowed, i + j);
  }
  assertEquals(i + 4, Derived.getX(allowed));

  d8.test.setAccessPolicy(allowed, false);
  assertThrows(() => Derived.setX(allowed, 999), TypeError);
  assertThrows(() => Derived.getX(allowed), TypeError);
  assertThrows(() => Derived.callM(allowed), TypeError);

  d8.test.setAccessPolicy(allowed, true);
  assertEquals(i + 4, Derived.getX(allowed));

  const denied = d8.test.createAccessCheckedObject(false);
  assertThrows(() => new Derived(denied), TypeError);
}
