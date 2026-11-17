// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class C {
  static #c = C;
  static #m() { return this; }
  static get #g() { return this.#m; }
  static test(C) {
    assertEquals(C?.#m(), C);
    assertEquals((C?.#m)(), C);
    assertEquals(C?.#c?.#m(), C);
    assertEquals((C?.#c?.#m)(), C);

    assertEquals(C?.#m(42), C);
    assertEquals((C?.#m)(42), C);
    assertEquals(C?.#c?.#m(42), C);
    assertEquals((C?.#c?.#m)(42), C);

    assertEquals(C?.#g(), C);
    assertEquals((C?.#g)(), C);
    assertEquals(C?.#c?.#g(), C);
    assertEquals((C?.#c?.#g)(), C);

    assertEquals(C?.#g(42), C);
    assertEquals((C?.#g)(42), C);
    assertEquals(C?.#c?.#g(42), C);
    assertEquals((C?.#c?.#g)(42), C);
    assertEquals((C?.#g)(1, ...[2], 3), C);
  }
}

C.test(C);
