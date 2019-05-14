// Copyright 2012 the V8 project authors. All rights reserved.
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

// Test CompareIC stubs for normal and strict equality comparison of known
// objects in hydrogen.

function lt(a, b) {
  return a < b;
}

function gt(a, b) {
  return a > b;
}

function eq(a, b) {
  return a == b;
}

function eq_strict(a, b) {
  return a === b;
}

function test(a, b, less, greater) {
  // Check CompareIC for equality of known objects.
  assertTrue(eq(a, a));
  assertTrue(eq(b, b));
  assertFalse(eq(a, b));
  assertTrue(eq_strict(a, a));
  assertTrue(eq_strict(b, b));
  assertFalse(eq_strict(a, b));
  assertEquals(lt(a, b), less);
  assertEquals(gt(a, b), greater);
  assertEquals(lt(b, a), greater);
  assertEquals(gt(b, a), less);
}

var obj1 = {toString: function() {return "1";}};
var obj2 = {toString: function() {return "2";}};

var less = obj1 < obj2;
var greater = obj1 > obj2;

test(obj1, obj2, less, greater);
test(obj1, obj2, less, greater);
test(obj1, obj2, less, greater);
%OptimizeFunctionOnNextCall(test);
test(obj1, obj2, less, greater);
test(obj1, obj2, less, greater);

obj1.x = 1;
test(obj1, obj2, less, greater);

obj2.y = 2;
test(obj1, obj2, less, greater);

var obj1 = {test: 3};
var obj2 = {test2: 3};

var less = obj1 < obj2;
var greater = obj1 > obj2;

test(obj1, obj2, less, greater);
test(obj1, obj2, less, greater);
test(obj1, obj2, less, greater);
%OptimizeFunctionOnNextCall(test);
test(obj1, obj2, less, greater);
test(obj1, obj2, less, greater);

obj1.toString = function() {return "1"};
var less = obj1 < obj2;
var greater = obj1 > obj2;
test(obj1, obj2, less, greater);
%OptimizeFunctionOnNextCall(test);
test(obj1, obj2, less, greater);

obj2.toString = function() {return "2"};
var less = true;
var greater = false;

test(obj1, obj2, less, greater);
obj2.y = 2;
test(obj1, obj2, less, greater);
