// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods

class C {
  set #a(val) {}
  setA(obj, val) { obj.#a = val; }

  constructor() {
    class D {
      get #a() {}
    }
    this.setA(new D(), 1);
  }
}

new C;
