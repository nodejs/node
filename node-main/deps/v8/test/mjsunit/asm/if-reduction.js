// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module() {
  "use asm";

  function if0() {
    var x = 0;
    x = 0 ? 11 : 12;
    return ((x | 0) == 11) | 0;
  }

  function if1() {
    var x = 0;
    x = 1 ? 13 : 14;
    return ((x | 0) == 13) | 0;
  }

  function if2() {
    var x = 0;
    x = 0 ? 15 : 16;
    return ((x | 0) != 15) | 0;
  }

  function if3() {
    var x = 0;
    x = 1 ? 17 : 18;
    return ((x | 0) != 17) | 0;
  }

  function if4() {
    var x = 0;
    var y = 0;
    x = 0 ? 19 : 20;
    y = ((x | 0) == 19) ? 21 : 22;
    return y | 0;
  }

  function if5() {
    var x = 0;
    var y = 0;
    x = 1 ? 23 : 24;
    y = ((x | 0) == 23) ? 25 : 26;
    return y | 0;
  }

  function if6() {
    var x = 0;
    var y = 0;
    var z = 0;
    x = 0 ? 27 : 28;
    y = ((x | 0) == 27) ? 29 : 30;
    z = ((y | 0) == 29) ? 31 : 32;
    return z | 0;
  }

  function if7() {
    var x = 0;
    var y = 0;
    var z = 0;
    var w = 0;
    x = 1 ? 33 : 34;
    y = ((x | 0) == 33) ? 35 : 36;
    z = ((y | 0) == 35) ? 37 : 38;
    w = ((z | 0) == 37) ? 39 : 40;
    return w | 0;
  }

  function if8() {
    var w = 0;
    var x = 0;
    var y = 0;
    var z = 0;
    if (0) {
      x = 0 ? 43 : 44;
      y = ((x | 0) == 43) ? 45 : 46;
      z = ((y | 0) == 45) ? 47 : 48;
      w = ((z | 0) == 47) ? 49 : 50;
    } else {
      x = 1 ? 53 : 54;
      y = ((x | 0) == 53) ? 55 : 56;
      z = ((y | 0) == 55) ? 57 : 58;
      w = ((z | 0) == 57) ? 59 : 60;
    }
    return w | 0;
  }

  return {if0: if0, if1: if1, if2: if2, if3: if3, if4: if4, if5: if5, if6: if6, if7: if7, if8: if8 };
}

var m = Module();
assertEquals(0, m.if0());
assertEquals(1, m.if1());
assertEquals(1, m.if2());
assertEquals(0, m.if3());
assertEquals(22, m.if4());
assertEquals(25, m.if5());
assertEquals(32, m.if6());
assertEquals(39, m.if7());
assertEquals(59, m.if8());


function Spec0(stdlib, foreign, heap) {
  "use asm";

  var xx = foreign.a | 0;
  var yy = foreign.b | 0;

  function f() {
    var x = 0;
    var y = 0;
    var z = 0;
    var w = 0;
    if (xx) {
      x = yy ? 43 : 44;
      y = ((x | 0) == 43) ? 45 : 46;
      z = ((y | 0) == 45) ? 47 : 48;
      w = ((z | 0) == 47) ? 49 : 50;
    } else {
      x = yy ? 53 : 54;
      y = ((x | 0) == 53) ? 55 : 56;
      z = ((y | 0) == 55) ? 57 : 58;
      w = ((z | 0) == 57) ? 59 : 60;
    }
    return w | 0;
  }
  return {f: f};
}
var Spec = (a, b, c) => Spec0(this, {a: a, b: b, c: c});

assertEquals(60, Spec(0,0).f());
assertEquals(59, Spec(0,1).f());
assertEquals(50, Spec(1,0).f());
assertEquals(49, Spec(1,1).f());
