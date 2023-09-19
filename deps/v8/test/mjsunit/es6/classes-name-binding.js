// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestNameBindingConst() {
  assertThrows('class C { constructor() { C = 42; } }; new C();', TypeError);
  assertThrows('new (class C { constructor() { C = 42; } })', TypeError);
  assertThrows('class C { m() { C = 42; } }; new C().m()', TypeError);
  assertThrows('new (class C { m() { C = 42; } }).m()', TypeError);
  assertThrows('class C { get x() { C = 42; } }; new C().x', TypeError);
  assertThrows('(new (class C { get x() { C = 42; } })).x', TypeError);
  assertThrows('class C { set x(_) { C = 42; } }; new C().x = 15;', TypeError);
  assertThrows('(new (class C { set x(_) { C = 42; } })).x = 15;', TypeError);
})();

(function TestNameBinding() {
  var C2;
  class C {
    constructor() {
      C2 = C;
    }
    m() {
      C2 = C;
    }
    get x() {
      C2 = C;
    }
    set x(_) {
      C2 = C;
    }
  }
  new C();
  assertEquals(C, C2);

  C2 = undefined;
  new C().m();
  assertEquals(C, C2);

  C2 = undefined;
  new C().x;
  assertEquals(C, C2);

  C2 = undefined;
  new C().x = 1;
  assertEquals(C, C2);
})();

(function TestNameBindingExpression() {
  var C3;
  var C = class C2 {
    constructor() {
      assertEquals(C2, C);
      C3 = C2;
    }
    m() {
      assertEquals(C2, C);
      C3 = C2;
    }
    get x() {
      assertEquals(C2, C);
      C3 = C2;
    }
    set x(_) {
      assertEquals(C2, C);
      C3 = C2;
    }
  }
  new C();
  assertEquals(C, C3);

  C3 = undefined;
  new C().m();
  assertEquals(C, C3);

  C3 = undefined;
  new C().x;
  assertEquals(C, C3);

  C3 = undefined;
  new C().x = 1;
  assertEquals(C, C3);
})();

(function TestNameBindingInExtendsExpression() {
  assertThrows(function() {
    class x extends x {}
  }, ReferenceError);

  assertThrows(function() {
    (class x extends x {});
  }, ReferenceError);

  assertThrows(function() {
    var x = (class x extends x {});
  }, ReferenceError);
})();
