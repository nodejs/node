// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ns from './modules-namespace-super-set-tdz-with-accessor.mjs';

let setterValue;

class A {
  set foo(v) { setterValue = v; }
  constructor() { return ns; }
}
class B extends A {
  constructor() {
    super();
    super.foo = 14;
  }
}

new B();
assertEquals(14, setterValue);

export let foo = 42;
