// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy

class A {
  static #a = 1;
  static b = class {
    static get_A() { return val.#a; }
  }
}
