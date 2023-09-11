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

var y = 3;

function get_y() { return this.y; }
function strict_get_y() { "use strict"; return this.y; }

// Test calls to strict mode function as methods.
for (var i = 0; i < 10; i++) assertEquals(3, strict_get_y.call(this));
var o = { y: 42 };
for (var i = 0; i < 10; i++) assertEquals(42, strict_get_y.call(o));

// Test calls to strict mode function with implicit receiver.
function g() {
  var exception = false;
  try { strict_get_y(); } catch(e) { exception = true; }
  assertTrue(exception);
}
for (var i = 0; i < 3; i++) g();

// Test calls to local strict mode function with implicit receiver.
function local_function_test() {
  function get_y() { return this.y; }
  function strict_get_y() { "use strict"; return this.y; }
  assertEquals(3, get_y());
  assertEquals(3, get_y(23));
  var exception = false;
  try {
    strict_get_y();
  } catch(e) {
    exception = true;
  }
  assertTrue(exception);
}

for (var i = 0; i < 10; i++) {
  local_function_test();
}

// Test call to catch variable strict-mode function. Implicit
// receiver.
var exception = false;
try {
  throw strict_get_y;
} catch(f) {
  try {
    f();
  } catch(e) {
    exception = true;
  }
  assertTrue(exception);
}


// Test calls to strict-mode function with the object from a with
// statement as the receiver.
with(this) {
  assertEquals(3, strict_get_y());
  assertEquals(3, strict_get_y(32));
}

var o = { y: 27 };
o.f = strict_get_y;
with(o) {
  assertEquals(27, f());
  assertEquals(27, f(23));
}


// Check calls to eval within a function with 'undefined' as receiver.
function implicit_receiver_eval() {
  "use strict";
  return eval("this");
}

assertEquals(void 0, implicit_receiver_eval());
assertEquals(void 0, implicit_receiver_eval(32));


// Strict mode function to get inlined.
function strict_return_receiver() {
  "use strict";
  return this;
}

// Inline with implicit receiver.
function g() {
  return strict_return_receiver();
}
%PrepareFunctionForOptimization(g);

for (var i = 0; i < 5; i++) {
  assertEquals(void 0, g());
  assertEquals(void 0, g(42));
}
%OptimizeFunctionOnNextCall(g);
assertEquals(void 0, g(42));
assertEquals(void 0, g());

// Inline with explicit receiver.
function g2() {
  var o = {};
  o.f = strict_return_receiver;
  return o.f();
}
%PrepareFunctionForOptimization(g2);

for (var i = 0; i < 5; i++) {
  assertTrue(typeof g2() == "object");
  assertTrue(typeof g2(42) == "object");
}
%OptimizeFunctionOnNextCall(g2);
assertTrue(typeof g2() == "object");
assertTrue(typeof g2(42) == "object");

// Test calls of aliased eval.
function outer_eval_receiver() {
  var eval = function() { return this; }
  function inner_strict() {
    "use strict";
    assertEquals('object', typeof eval());
  }
  inner_strict();
}
outer_eval_receiver();

function outer_eval_conversion3(eval, expected) {
  function inner_strict() {
    "use strict";
    var x = eval("this");
    assertEquals(expected, typeof x);
  }
  inner_strict();
}

function strict_return_this() { "use strict"; return this; }
function return_this() { return this; }
function strict_eval(s) { "use strict"; return eval(s); }
function non_strict_eval(s) { return eval(s); }

outer_eval_conversion3(strict_return_this, 'undefined');
outer_eval_conversion3(return_this, 'object');
outer_eval_conversion3(strict_eval, 'undefined');
outer_eval_conversion3(non_strict_eval, 'object');

outer_eval_conversion3(eval, 'undefined');

function test_constant_function() {
  var o = { f: function() { "use strict"; return this; } };
  this.__proto__ = o;
  for (var i = 0; i < 10; i++) assertEquals(void 0, f());
}
test_constant_function();

function test_field() {
  var o = { };
  o.f = function() {};
  o.f = function() { "use strict"; return this; };
  this.__proto__ = o;
  for (var i = 0; i < 10; i++) assertEquals(void 0, f());
}
test_field();
