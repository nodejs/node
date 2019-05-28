// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-private-fields

class Y {
  makeClass() {
    return class {
      setA(val) { this.#a = val; }
      getA() { return this.#a; }
      getB() { return this.#b; }
    }
  }
  #a;
}
