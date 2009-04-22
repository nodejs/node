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

assertEquals(void 0, eval());
assertEquals(4, eval(4));

function f() { return 'The f function'; };
assertTrue(f === eval(f));

function g(x, y) { return 4; };

count = 0;
assertEquals(4, eval('2 + 2', count++));
assertEquals(1, count);

try {
  eval('hest 7 &*^*&^');
  assertTrue(false, 'Did not throw on syntax error.');
} catch (e) {
  assertEquals('SyntaxError', e.name);
}


// eval has special evaluation order for consistency with other browsers.
global_eval = eval;
assertEquals(void 0, eval(eval("var eval = function f(x) { return 'hest';}")))
eval = global_eval;

//Test eval with different number of parameters.
global_eval = eval;
eval = function(x, y) { return x + y; };
assertEquals(4, eval(2, 2));
eval = global_eval;

// Test that un-aliased eval reads from local context.
foo = 0;
result = 
  (function() {
    var foo = 2;
    return eval('foo');
  })();
assertEquals(2, result);

//Test that un-aliased eval writes to local context.
foo = 0;
result = 
  (function() {
    var foo = 1;
    eval('foo = 2');
    return foo;
  })();
assertEquals(2, result);
assertEquals(0, foo);

// Test that un-aliased eval has right receiver.
function MyObject() { this.self = eval('this'); }
var o = new MyObject();
assertTrue(o === o.self);

// Test that aliased eval reads from global context.
var e = eval;
foo = 0;
result = 
  (function() {
    var foo = 2;
    return e('foo');
  })();
assertEquals(0, result);

// Test that aliased eval writes to global context.
var e = eval;
foo = 0;
(function() { e('var foo = 2;'); })();
assertEquals(2, foo);

// Test that aliased eval has right receiver.
function MyOtherObject() { this.self = e('this'); }
var o = new MyOtherObject();
assertTrue(this === o.self);

// Try to cheat the 'aliased eval' detection.
var x = this;
foo = 0;
result = 
  (function() {
    var foo = 2;
    return x.eval('foo');
  })();
assertEquals(0, result);

foo = 0;
result = 
  (function() {
    var eval = function(x) { return x; };
    var foo = eval(2);
    return e('foo');
  })();
assertEquals(0, result);

result =
  (function() {
    var eval = function(x) { return 2 * x; };
    return (function() { return eval(2); })();
  })();
assertEquals(4, result);

eval = function(x) { return 2 * x; };
result = 
  (function() {
    return (function() { return eval(2); })();
  })();
assertEquals(4, result);
