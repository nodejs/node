// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function Method() {
  class C {
    eval() {
      return 1;
    }
    arguments() {
      return 2;
    }
    static eval() {
      return 3;
    }
    static arguments() {
      return 4;
    }
  };

  assertEquals(1, new C().eval());
  assertEquals(2, new C().arguments());
  assertEquals(3, C.eval());
  assertEquals(4, C.arguments());
})();


(function Getters() {
  class C {
    get eval() {
      return 1;
    }
    get arguments() {
      return 2;
    }
    static get eval() {
      return 3;
    }
    static get arguments() {
      return 4;
    }
  };

  assertEquals(1, new C().eval);
  assertEquals(2, new C().arguments);
  assertEquals(3, C.eval);
  assertEquals(4, C.arguments);
})();


(function Setters() {
  var x = 0;
  class C {
    set eval(v) {
      x = v;
    }
    set arguments(v) {
      x = v;
    }
    static set eval(v) {
      x = v;
    }
    static set arguments(v) {
      x = v;
    }
  };

  new C().eval = 1;
  assertEquals(1, x);
  new C().arguments = 2;
  assertEquals(2, x);
  C.eval = 3;
  assertEquals(3, x);
  C.arguments = 4;
  assertEquals(4, x);
})();
