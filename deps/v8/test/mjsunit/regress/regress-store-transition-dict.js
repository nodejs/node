// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function SetX(o, v) {
    o.x = v;
  }

  function SetY(o, v) {
    o.y = v;
  }

  var p = {};

  function Create() {
    var o = {__proto__:p, b:1, a:2};
    delete o.b;
    assertFalse(%HasFastProperties(o));
    return o;
  }

  for (var i = 0; i < 10; i++) {
    var o = Create();
    SetX(o, 13);
    SetY(o, 13);
  }

  Object.defineProperty(p, "x", {value:42, configurable: true, writable: false});

  for (var i = 0; i < 10; i++) {
    var o = Create();
    SetY(o, 13);
  }

  var o = Create();
  assertEquals(42, o.x);
  SetX(o, 13);
  assertEquals(42, o.x);
})();


(function() {
  var p1 = {a:10};
  Object.defineProperty(p1, "x", {value:42, configurable: true, writable: false});

  var p2 = {__proto__: p1, x:153};
  for (var i = 0; i < 2000; i++) {
    p1["p" + i] = 0;
    p2["p" + i] = 0;
  }
  assertFalse(%HasFastProperties(p1));
  assertFalse(%HasFastProperties(p2));

  function GetX(o) {
    return o.x;
  }
  function SetX(o, v) {
    o.x = v;
  }

  function Create() {
    var o = {__proto__:p2, b:1, a:2};
    return o;
  }

  for (var i = 0; i < 10; i++) {
    var o = Create();
    assertEquals(153, GetX(o));
    SetX(o, 13);
    assertEquals(13, GetX(o));
  }

  delete p2.x;
  assertFalse(%HasFastProperties(p1));
  assertFalse(%HasFastProperties(p2));

  var o = Create();
  assertEquals(42, GetX(o));
  SetX(o, 13);
  assertEquals(42, GetX(o));
})();
