// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-methods
// This tests that empty inner classes don't assign private brands of outer
// classes in their instances after scope chain deserialization.

'use strict';

class Outer {
  constructor() {}
#method(){}
  factory() {
    class Inner {
      constructor() {}
    }
    return Inner;
  }
  run(obj) {
    obj.#method();
  }
}

const instance = new Outer();
const Inner = instance.factory();
// It should not pass the brand check.
assertThrows(() => instance.run(new Inner()), TypeError);
// It should pass the brand check.
instance.run(new Outer());
