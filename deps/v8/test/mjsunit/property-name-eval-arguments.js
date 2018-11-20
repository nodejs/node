// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function TestSloppyMode() {
  var e = 1, a = 2;
  var o = {
    get eval() {
      return e;
    },
    set eval(v) {
      e = v;
    },
    get arguments() {
      return a;
    },
    set arguments(v) {
      a = v;
    },
  };

  assertEquals(1, o.eval);
  o.eval = 3;
  assertEquals(3, e);

  assertEquals(2, o.arguments);
  o.arguments = 4;
  assertEquals(4, a);
})();


(function TestStrictMode() {
  'use strict';

  var e = 1, a = 2;
  var o = {
    get eval() {
      return e;
    },
    set eval(v) {
      e = v;
    },
    get arguments() {
      return a;
    },
    set arguments(v) {
      a = v;
    },
  };

  assertEquals(1, o.eval);
  o.eval = 3;
  assertEquals(3, e);

  assertEquals(2, o.arguments);
  o.arguments = 4;
  assertEquals(4, a);
})();
