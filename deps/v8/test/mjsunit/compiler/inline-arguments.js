// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --max-opt-count=100

function A() {
}

A.prototype.X = function (a, b, c) {
  assertTrue(this instanceof A);
  assertEquals(1, a);
  assertEquals(2, b);
  assertEquals(3, c);
};

A.prototype.Y = function () {
  this.X.apply(this, arguments);
};

A.prototype.Z = function () {
  this.Y(1,2,3);
};

var a = new A();
a.Z(4,5,6);
a.Z(4,5,6);
%OptimizeFunctionOnNextCall(a.Z);
a.Z(4,5,6);
A.prototype.X.apply = function (receiver, args) {
  return Function.prototype.apply.call(this, receiver, args);
};
a.Z(4,5,6);


// Ensure that HArgumentsObject is inserted in a correct place
// and dominates all uses.
function F1() { }
function F2() { F1.apply(this, arguments); }
function F3(x, y) {
  if (x) {
    F2(y);
  }
}

function F31() {
  return F1.apply(this, arguments);
}

function F4() {
  F3(true, false);
  return F31(1);
}

F4(1);
F4(1);
F4(1);
%OptimizeFunctionOnNextCall(F4);
F4(1);


// Test correct adapation of arguments.
// Strict mode prevents arguments object from shadowing parameters.
(function () {
  "use strict";

  function G2() {
    assertArrayEquals([1,2], arguments);
  }

  function G4() {
    assertArrayEquals([1,2,3,4], arguments);
  }

  function adapt2to4(a, b, c, d) {
    G2.apply(this, arguments);
  }

  function adapt4to2(a, b) {
    G4.apply(this, arguments);
  }

  function test_adaptation() {
    adapt2to4(1, 2);
    adapt4to2(1, 2, 3, 4);
  }

  test_adaptation();
  test_adaptation();
  %OptimizeFunctionOnNextCall(test_adaptation);
  test_adaptation();
})();

// Test arguments access from the inlined function.
%NeverOptimizeFunction(uninlinable);
function uninlinable(v) {
  assertEquals(0, v);
  return 0;
}

function toarr_inner() {
  var a = arguments;
  var marker = a[0];
  uninlinable(uninlinable(0, 0), marker.x);

  var r = new Array();
  for (var i = a.length - 1; i >= 1; i--) {
    r.push(a[i]);
  }

  return r;
}

function toarr1(marker, a, b, c) {
  return toarr_inner(marker, a / 2, b / 2, c / 2);
}

function toarr2(marker, a, b, c) {
  var x = 0;
  return uninlinable(uninlinable(0, 0),
                     x = toarr_inner(marker, a / 2, b / 2, c / 2)), x;
}

function test_toarr(toarr) {
  var marker = { x: 0 };
  assertArrayEquals([3, 2, 1], toarr(marker, 2, 4, 6));
  assertArrayEquals([3, 2, 1], toarr(marker, 2, 4, 6));
  %OptimizeFunctionOnNextCall(toarr);
  assertArrayEquals([3, 2, 1], toarr(marker, 2, 4, 6));
  delete marker.x;
  assertArrayEquals([3, 2, 1], toarr(marker, 2, 4, 6));
}

test_toarr(toarr1);
test_toarr(toarr2);


// Test that arguments access from inlined function uses correct values.
(function () {
  function inner(x, y) {
    "use strict";
    x = 10;
    y = 20;
    for (var i = 0; i < 1; i++) {
      for (var j = 1; j <= arguments.length; j++) {
        return arguments[arguments.length - j];
      }
    }
  }

  function outer(x, y) {
    return inner(x, y);
  }

  %OptimizeFunctionOnNextCall(outer);
  %OptimizeFunctionOnNextCall(inner);
  assertEquals(2, outer(1, 2));
})();


(function () {
  function inner(x, y) {
    "use strict";
    x = 10;
    y = 20;
    for (var i = 0; i < 1; i++) {
      for (var j = 1; j <= arguments.length; j++) {
        return arguments[arguments.length - j];
      }
    }
  }

  function outer(x, y) {
    return inner(x, y);
  }

  assertEquals(2, outer(1, 2));
  assertEquals(2, outer(1, 2));
  assertEquals(2, outer(1, 2));
  %OptimizeFunctionOnNextCall(outer);
  assertEquals(2, outer(1, 2));
})();


// Test inlining and deoptimization of functions accessing and modifying
// the arguments object in strict mode with mismatched arguments count.
(function () {
  "use strict";
  function test(outerCount, middleCount, innerCount) {
    var forceDeopt = { deopt:false };
    function inner(x,y) {
      x = 0; y = 0;
      forceDeopt.deopt;
      assertSame(innerCount, arguments.length);
      for (var i = 0; i < arguments.length; i++) {
        assertSame(30 + i, arguments[i]);
      }
    }

    function middle(x,y) {
      x = 0; y = 0;
      if (innerCount == 1) inner(30);
      if (innerCount == 2) inner(30, 31);
      if (innerCount == 3) inner(30, 31, 32);
      assertSame(middleCount, arguments.length);
      for (var i = 0; i < arguments.length; i++) {
        assertSame(20 + i, arguments[i]);
      }
    }

    function outer(x,y) {
      x = 0; y = 0;
      if (middleCount == 1) middle(20);
      if (middleCount == 2) middle(20, 21);
      if (middleCount == 3) middle(20, 21, 22);
      assertSame(outerCount, arguments.length);
      for (var i = 0; i < arguments.length; i++) {
        assertSame(10 + i, arguments[i]);
      }
    }

    for (var step = 0; step < 4; step++) {
      if (outerCount == 1) outer(10);
      if (outerCount == 2) outer(10, 11);
      if (outerCount == 3) outer(10, 11, 12);
      if (step == 1) %OptimizeFunctionOnNextCall(outer);
      if (step == 2) delete forceDeopt.deopt;
    }

    %DeoptimizeFunction(outer);
    %DeoptimizeFunction(middle);
    %DeoptimizeFunction(inner);
    %ClearFunctionTypeFeedback(outer);
    %ClearFunctionTypeFeedback(middle);
    %ClearFunctionTypeFeedback(inner);
  }

  for (var a = 1; a <= 3; a++) {
    for (var b = 1; b <= 3; b++) {
      for (var c = 1; c <= 3; c++) {
        test(a,b,c);
      }
    }
  }
})();


// Test materialization of arguments object with values in registers.
(function () {
  "use strict";
  var forceDeopt = { deopt:false };
  function inner(a,b,c,d,e,f,g,h,i,j) {
    var args = arguments;
    forceDeopt.deopt;
    assertSame(10, args.length);
    assertSame(a, args[0]);
    assertSame(b, args[1]);
    assertSame(c, args[2]);
    assertSame(d, args[3]);
    assertSame(e, args[4]);
    assertSame(f, args[5]);
    assertSame(g, args[6]);
    assertSame(h, args[7]);
    assertSame(i, args[8]);
    assertSame(j, args[9]);
  }

  var a = 0.5;
  var b = 1.7;
  var c = 123;
  function outer() {
    inner(
      a - 0.3,  // double in double register
      b + 2.3,  // integer in double register
      c + 321,  // integer in general register
      c - 456,  // integer in stack slot
      a + 0.1, a + 0.2, a + 0.3, a + 0.4, a + 0.5,
      a + 0.6   // double in stack slot
    );
  }

  outer();
  outer();
  %OptimizeFunctionOnNextCall(outer);
  outer();
  delete forceDeopt.deopt;
  outer();
})();


// Test inlining of functions with %_Arguments and %_ArgumentsLength intrinsic.
(function () {
  function inner(len,a,b,c) {
    assertSame(len, %_ArgumentsLength());
    for (var i = 1; i < len; ++i) {
      var c = String.fromCharCode(96 + i);
      assertSame(c, %_Arguments(i));
    }
  }

  function outer() {
    inner(1);
    inner(2, 'a');
    inner(3, 'a', 'b');
    inner(4, 'a', 'b', 'c');
    inner(5, 'a', 'b', 'c', 'd');
    inner(6, 'a', 'b', 'c', 'd', 'e');
  }

  outer();
  outer();
  %OptimizeFunctionOnNextCall(outer);
  outer();
})();
