// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc


class Derived extends RegExp {
  constructor(a) {
    // Syntax Error
    const a = 1;
  }
}

let o = Reflect.construct(RegExp, [], Derived);
%HeapObjectVerify(o);
// Check that we can properly access lastIndex.
assertEquals(o.lastIndex, 0);
o.lastIndex = 1;
assertEquals(o.lastIndex, 1);
o.lastIndex = 0;
gc();
