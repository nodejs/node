// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function outer() {
  function* generator() {
    let arrow = () => {
      assertSame(expectedReceiver, this);
      assertEquals(42, arguments[0]);
    };
    arrow();
  }
  generator.call(this, 42).next();
}
let expectedReceiver = {};
outer.call(expectedReceiver);
