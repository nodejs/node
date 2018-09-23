// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

'use strong';


function desc(obj, n) {
  return Object.getOwnPropertyDescriptor(obj, n);
}


(function TestClass() {
  class C {
    m() {
      super.x;
    }
    get x() {
      super.x;
    }
    set y(_) {
      super.x;
    }
    static m() {
      super.x;
    }
    static get x() {
      super.x;
    }
    static set y(_) {
      super.x;
    }
  }

  assertEquals(C.prototype, C.prototype.m[%HomeObjectSymbol()]);
  assertEquals(C.prototype, desc(C.prototype, 'x').get[%HomeObjectSymbol()]);
  assertEquals(C.prototype, desc(C.prototype, 'y').set[%HomeObjectSymbol()]);
  assertEquals(C, C.m[%HomeObjectSymbol()]);
  assertEquals(C, desc(C, 'x').get[%HomeObjectSymbol()]);
  assertEquals(C, desc(C, 'y').set[%HomeObjectSymbol()]);
})();


(function TestObjectLiteral() {
  let o = {
    m() {
      super.x;
    },
    get x() {
      super.x;
    },
    set y(_) {
      super.x;
    }
  };

  assertEquals(o, o.m[%HomeObjectSymbol()]);
  assertEquals(o, desc(o, 'x').get[%HomeObjectSymbol()]);
  assertEquals(o, desc(o, 'y').set[%HomeObjectSymbol()]);
})();
