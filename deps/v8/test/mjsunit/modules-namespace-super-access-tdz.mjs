// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ns from './modules-namespace-super-access-tdz.mjs';

class A { constructor() { return ns; } }
class B extends A {
  constructor() {
    super();
    super.foo = 14;
  }
}

assertThrows(() => new B(), ReferenceError);

export let foo = 42;
