// Copyright 2008 the V8 project authors. All rights reserved.
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

// This test attempts to test the inline caching for keyed access.

// ----------------------------------------------------------------------
// Prototype accessor.
// ----------------------------------------------------------------------
var runTest = function() {
  var initial_P = 'prototype';
  var P = initial_P;
  var H = 'hasOwnProperty';

  var f = function() {};

  function prototypeTest(change_index) {
    for (var i = 0; i < 10; i++) {
      var property = f[P];
      if (i <= change_index) {
        assertEquals(f.prototype, property);
      } else {
        assertEquals(f.hasOwnProperty, property);
      }
      if (i == change_index) P = H;
    }
    P = initial_P;
  }

  for (var i = 0; i < 10; i++) prototypeTest(i);

  f.prototype = 43;

  for (var i = 0; i < 10; i++) prototypeTest(i);
}

runTest();

// ----------------------------------------------------------------------
// Array length accessor.
// ----------------------------------------------------------------------
runTest = function() {
  var initial_L = 'length';
  var L = initial_L;
  var zero = '0';

  var a = new Array(10);

  function arrayLengthTest(change_index) {
    for (var i = 0; i < 10; i++) {
      var l = a[L];
      if (i <= change_index) {
        assertEquals(10, l);
      } else {
        assertEquals(undefined, l);
      }
      if (i == change_index) L = zero;
    }
    L = initial_L;
  }

  for (var i = 0; i < 10; i++) arrayLengthTest(i);
}

runTest();

// ----------------------------------------------------------------------
// String length accessor.
// ----------------------------------------------------------------------
runTest = function() {
  var initial_L = 'length';
  var L = initial_L;
  var zero = '0';

  var s = "asdf"

  function stringLengthTest(change_index) {
    for (var i = 0; i < 10; i++) {
      var l = s[L];
      if (i <= change_index) {
        assertEquals(4, l);
      } else {
        assertEquals('a', l);
      }
      if (i == change_index) L = zero;
    }
    L = initial_L;
  }

  for (var i = 0; i < 10; i++) stringLengthTest(i);
}

runTest();

// ----------------------------------------------------------------------
// Field access.
// ----------------------------------------------------------------------
runTest = function() {
  var o = { x: 42, y: 43 }

  var initial_X = 'x';
  var X = initial_X;
  var Y = 'y';

  function fieldTest(change_index) {
    for (var i = 0; i < 10; i++) {
      var property = o[X];
      if (i <= change_index) {
        assertEquals(42, property);
      } else {
        assertEquals(43, property);
      }
      if (i == change_index) X = Y;
    }
    X = initial_X;
  };

  for (var i = 0; i < 10; i++) fieldTest(i);
}

runTest();


// ----------------------------------------------------------------------
// Constant function access.
// ----------------------------------------------------------------------
runTest = function() {
  function fun() { };

  var o = new Object();
  o.f = fun;
  o.x = 42;

  var initial_F = 'f';
  var F = initial_F;
  var X = 'x'

  function constantFunctionTest(change_index) {
    for (var i = 0; i < 10; i++) {
      var property = o[F];
      if (i <= change_index) {
        assertEquals(fun, property);
      } else {
        assertEquals(42, property);
      }
      if (i == change_index) F = X;
    }
    F = initial_F;
  };

  for (var i = 0; i < 10; i++) constantFunctionTest(i);
}

runTest();

// ----------------------------------------------------------------------
// Keyed store field.
// ----------------------------------------------------------------------

runTest = function() {
  var o = { x: 42, y: 43 }

  var initial_X = 'x';
  var X = initial_X;
  var Y = 'y';

  function fieldTest(change_index) {
    for (var i = 0; i < 10; i++) {
      o[X] = X;
      var property = o[X];
      if (i <= change_index) {
        assertEquals('x', property);
      } else {
        assertEquals('y', property);
      }
      if (i == change_index) X = Y;
    }
    X = initial_X;
  };

  for (var i = 0; i < 10; i++) fieldTest(i);
}

runTest();
