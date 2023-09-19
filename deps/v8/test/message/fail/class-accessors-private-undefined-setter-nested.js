// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class C {
  get #a() {}
  getA(obj) { return obj.#a; }

  constructor() {
    class D {
      set #a(val) {}
    }
    this.getA(new D());
  }
}

new C;
