// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class A {
    static #foo = 3;
    constructor() {
      print(A.prototype.#foo);
    }
}

assertThrows(() => new A(), TypeError);

class B {
  static #foo = 3;
  constructor() {
    B.prototype.#foo = 2;
  }
}

assertThrows(() => new B(), TypeError);
