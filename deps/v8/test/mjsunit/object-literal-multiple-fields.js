// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestConstants() {
  var o = {
    p: 1,
    p: 2,
  };
  assertEquals(2, o.p);
})();


(function TestMaterialized() {
  var o = {
    p: [1],
    p: [2],
  };
  assertEquals(2, o.p[0]);
})();


(function TestMaterialize2() {
  var o = {
    p: function() {},
    p: 2,
  };
  assertEquals(2, o.p);
})();



(function TestComputed() {
  var o = {
    p: (function() { return 1; })(),
    p: (function() { return 2; })(),
  };
  assertEquals(2, o.p);
})();


(function TestComputed2() {
  var o = {
    p: (function() { return 1; })(),
    p: 2,
  };
  assertEquals(2, o.p);
})();



(function TestGetter() {
  var o = {
    get p() { return 1; },
    get p() { return 2; },
  };
  assertEquals(2, o.p);
})();


(function TestGetterSetter() {
  var o = {
    get p() { return 1; },
    set p(_) {},
  };
  assertEquals(1, o.p);

  o = {
    set p(_) {},
    get p() { return 2; },
  };
  assertEquals(2, o.p);
})();


(function TestCombined() {
  var o = {
    get p() { return 1; },
    p: 2,
  };
  assertEquals(2, o.p);

  o = {
    get p() { return 1; },
    p: 2,
    get p() { return 3; },
  };
  assertEquals(3, o.p);

  o = {
    get p() { return 1; },
    p: 2,
    set p(_) {},
  };
  assertEquals(undefined, o.p);
})();
