// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class C1 {
  constructor() {
    const o = {
      boom() {
        class C2 extends super.constructor {
          static boom_s = 42();
        };
        return C2;
      },
    };
    o.boom();

    // Needed to make sure constructor has a context.
    function F17() {
      o;
    }
  }
}
assertThrows(() => { new C1(); }, TypeError);
