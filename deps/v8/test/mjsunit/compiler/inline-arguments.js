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

// Flags: --allow-natives-syntax

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
