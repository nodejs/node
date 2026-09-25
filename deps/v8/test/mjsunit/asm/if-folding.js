// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module() {
  "use asm";

  function if0() {
    if (0) return 11;
    return 12;
  }

  function if1() {
    if (1) return 13;
    return 14;
  }

  function if2() {
    if (0) return 15;
    else return 16;
    return 0;  // needed for validation
  }

  function if3() {
    if (1) return 17;
    else return 18;
    return 0;  // needed for validation
  }

  function if4() {
    return (1 ? 19 : 20) | 0;
  }

  function if5() {
    return (0 ? 21 : 22) | 0;
  }

  function if6() {
    var x = 0;
    x = 0 ? 23 : 24;
    return x | 0;
  }

  function if7() {
    var x = 0;
    if (0) {
      x = 0 ? 25 : 26;
    } else {
      x = 0 ? 27 : 28;
    }
    return x | 0;
  }

  function if8() {
    var x = 0;
    if (0) {
      if (0) {
        x = 0 ? 29 : 30;
      } else {
        x = 0 ? 31 : 32;
      }
    } else {
      if (0) {
        x = 0 ? 33 : 34;
      } else {
        x = 0 ? 35 : 36;
      }
    }
    return x | 0;
  }

  return {if0: if0, if1: if1, if2: if2, if3: if3, if4: if4, if5: if5, if6: if6, if7: if7, if8: if8 };
}

var m = Module();
assertEquals(12, m.if0());
assertEquals(13, m.if1());
assertEquals(16, m.if2());
assertEquals(17, m.if3());
assertEquals(19, m.if4());
assertEquals(22, m.if5());
assertEquals(24, m.if6());
assertEquals(28, m.if7());
assertEquals(36, m.if8());


function Spec0(stdlib, foreign, heap) {
  "use asm";

  var xx = foreign.a | 0;
  var yy = foreign.b | 0;
  var zz = foreign.c | 0;

  function f() {
    var x = 0;
    if (xx) {
      if (yy) {
        x = zz ? 29 : 30;
      } else {
        x = zz ? 31 : 32;
      }
    } else {
      if (yy) {
        x = zz ? 33 : 34;
      } else {
        x = zz ? 35 : 36;
      }
    }
    return x | 0;
  }
  return {f: f};
}
var Spec = (a, b, c) => Spec0(this, {a: a, b: b, c: c});

assertEquals(36, Spec(0,0,0).f());
assertEquals(35, Spec(0,0,1).f());
assertEquals(34, Spec(0,1,0).f());
assertEquals(33, Spec(0,1,1).f());
assertEquals(32, Spec(1,0,0).f());
assertEquals(31, Spec(1,0,1).f());
assertEquals(30, Spec(1,1,0).f());
assertEquals(29, Spec(1,1,1).f());
