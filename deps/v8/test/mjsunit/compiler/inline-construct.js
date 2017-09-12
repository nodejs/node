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

// Test inlining of constructor calls.

function TestInlinedConstructor(constructor, closure) {
  var result;
  var counter = { value:0 };
  var noDeopt = { deopt:0 };
  var forceDeopt = { /*empty*/ };

  result = closure(constructor, 11, noDeopt, counter);
  assertEquals(11, result);
  assertEquals(1, counter.value);

  result = closure(constructor, 23, noDeopt, counter);
  assertEquals(23, result);
  assertEquals(2, counter.value);

  %OptimizeFunctionOnNextCall(closure);
  result = closure(constructor, 42, noDeopt, counter);
  assertEquals(42, result);
  assertEquals(3, counter.value);

  result = closure(constructor, 127, forceDeopt, counter);
  assertEquals(127, result)
  assertEquals(4, counter.value);

  %DeoptimizeFunction(closure);
  %ClearFunctionFeedback(closure);
  %ClearFunctionFeedback(constructor);
}

function value_context(constructor, val, deopt, counter) {
  var obj = new constructor(val, deopt, counter);
  return obj.x;
}

function test_context(constructor, val, deopt, counter) {
  if (!new constructor(val, deopt, counter)) {
    assertUnreachable("should not happen");
  }
  return val;
}

function effect_context(constructor, val, deopt, counter) {
  new constructor(val, deopt, counter);
  return val;
}

function TestInAllContexts(constructor) {
  TestInlinedConstructor(constructor, value_context);
  TestInlinedConstructor(constructor, test_context);
  TestInlinedConstructor(constructor, effect_context);
}


// Test constructor returning nothing in all contexts.
function c1(val, deopt, counter) {
  deopt.deopt;
  this.x = val;
  counter.value++;
}
TestInAllContexts(c1);


// Test constructor returning an object in all contexts.
function c2(val, deopt, counter) {
  var obj = {};
  deopt.deopt;
  obj.x = val;
  counter.value++;
  return obj;
}
TestInAllContexts(c2);


// Test constructor returning a primitive value in all contexts.
function c3(val, deopt, counter) {
  deopt.deopt;
  this.x = val;
  counter.value++;
  return "not an object";
}
TestInAllContexts(c3);


// Test constructor called with too many arguments.
function c_too_many(a, b) {
  this.x = a + b;
}
function f_too_many(a, b, c) {
  var obj = new c_too_many(a, b, c);
  return obj.x;
}
assertEquals(23, f_too_many(11, 12, 1));
assertEquals(42, f_too_many(23, 19, 1));
%OptimizeFunctionOnNextCall(f_too_many);
assertEquals(43, f_too_many(1, 42, 1));
assertEquals("foobar", f_too_many("foo", "bar", "baz"))


// Test constructor called with too few arguments.
function c_too_few(a, b) {
  assertSame(undefined, b);
  this.x = a + 1;
}
function f_too_few(a) {
  var obj = new c_too_few(a);
  return obj.x;
}
assertEquals(12, f_too_few(11));
assertEquals(24, f_too_few(23));
%OptimizeFunctionOnNextCall(f_too_few);
assertEquals(2, f_too_few(1));
assertEquals("foo1", f_too_few("foo"))


// Test constructor that cannot be inlined.
function c_unsupported_syntax(val, deopt, counter) {
  try {
    deopt.deopt;
    this.x = val;
    counter.value++;
  } catch(e) {
    throw new Error();
  }
}
TestInAllContexts(c_unsupported_syntax);


// Regression test: Inlined constructors called as functions do not get their
// implicit receiver object set to undefined, even in strict mode.
function c_strict(val, deopt, counter) {
  "use strict";
  deopt.deopt;
  this.x = val;
  counter.value++;
}
TestInAllContexts(c_strict);
