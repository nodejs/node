// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-restrict-constructor-return --always-opt

class Base {
  constructor(x) {
    return x;
  }
}
class Derived extends Base {
  constructor(use, x) {
    super(use, x);
  }
}
assertThrows(() => new Derived(true, 5), TypeError);
