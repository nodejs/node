// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module() {
  "use asm";

  function if0() {
    var x = 0 ? 11 : 12;
    return (x == 11) | 0;
  }

  function if1() {
    var x = 1 ? 13 : 14;
    return (x == 13) | 0;
  }

  function if2() {
    var x = 0 ? 15 : 16;
    return (x != 15) | 0;
  }

  function if3() {
    var x = 1 ? 17 : 18;
    return (x != 17) | 0;
  }

  function if4() {
    var x = 0 ? 19 : 20;
    var y = (x == 19) ? 21 : 22;
    return y;
  }

  function if5() {
    var x = 1 ? 23 : 24;
    var y = (x == 23) ? 25 : 26;
    return y;
  }

  function if6() {
    var x = 0 ? 27 : 28;
    var y = (x == 27) ? 29 : 30;
    var z = (y == 29) ? 31 : 32;
    return z;
  }

  function if7() {
    var x = 1 ? 33 : 34;
    var y = (x == 33) ? 35 : 36;
    var z = (y == 35) ? 37 : 38;
    var w = (z == 37) ? 39 : 40;
    return w;
  }

  function if8() {
    if (0) {
      var x = 0 ? 43 : 44;
      var y = (x == 43) ? 45 : 46;
      var z = (y == 45) ? 47 : 48;
      var w = (z == 47) ? 49 : 50;
    } else {
      var x = 1 ? 53 : 54;
      var y = (x == 53) ? 55 : 56;
      var z = (y == 55) ? 57 : 58;
      var w = (z == 57) ? 59 : 60;
    }
    return w;
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


function Spec(a,b) {
  "use asm";

  var xx = a | 0;
  var yy = b | 0;

  function f() {
    if (xx) {
      var x = yy ? 43 : 44;
      var y = (x == 43) ? 45 : 46;
      var z = (y == 45) ? 47 : 48;
      var w = (z == 47) ? 49 : 50;
    } else {
      var x = yy ? 53 : 54;
      var y = (x == 53) ? 55 : 56;
      var z = (y == 55) ? 57 : 58;
      var w = (z == 57) ? 59 : 60;
    }
    return w;
  }
  return {f: f};
}

assertEquals(60, Spec(0,0).f());
assertEquals(59, Spec(0,1).f());
assertEquals(50, Spec(1,0).f());
assertEquals(49, Spec(1,1).f());
