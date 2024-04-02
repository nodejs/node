// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function StoreToSuper () {
  "use strict";
  class A {
    s() {
      super.bla = 10;
    }
  };

  let a = new A();
  (new A).s.call(a);
  assertEquals(10, a.bla);
  assertThrows(function() { (new A).s.call(undefined); }, TypeError);
  assertThrows(function() { (new A).s.call(42); }, TypeError);
  assertThrows(function() { (new A).s.call(null); }, TypeError);
  assertThrows(function() { (new A).s.call("abc"); }, TypeError);
})();


(function LoadFromSuper () {
  "use strict";
  class A {
    s() {
      return super.bla;
    }
  };

  let a = new A();
  assertSame(undefined, (new A).s.call(a));
  assertSame(undefined, (new A).s.call(undefined));
  assertSame(undefined, (new A).s.call(42));
  assertSame(undefined, (new A).s.call(null));
  assertSame(undefined, (new A).s.call("abc"));
})();
