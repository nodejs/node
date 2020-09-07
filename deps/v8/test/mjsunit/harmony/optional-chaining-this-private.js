// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class C {
  static #c = C;
  static #m() { return this; }
  static test(C) {
    assertEquals(C?.#m(), C);
    assertEquals((C?.#m)(), C);
    assertEquals(C?.#c?.#m(), C);
    assertEquals((C?.#c?.#m)(), C);
  }
}

C.test(C);
