// Copyright 2009 the V8 project authors. All rights reserved.
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

// Test some expression contexts involving short-circuit boolean
// operations that did not otherwise have test coverage.


var x = 42;

// Literals in value/test context.
assertEquals(x, function () { return 0 || x }());
assertEquals(1, function () { return 1 || x }());

// Literals in test/value context.
assertEquals(0, function () { return 0 && x }());
assertEquals(x, function () { return 1 && x }());

// A value on top of the stack in value/test context.
assertEquals(x, function(y) { return y++ || x }(0));
assertEquals(1, function(y) { return y++ || x }(1));

// A value on top of the stack in a test/value context.
assertEquals(0, function(y) { return y++ && x }(0));
assertEquals(x, function(y) { return y++ && x }(1));

// An object literal in value context.
assertEquals(0, function () { return {x: 0}}().x);

// An object literal in value/test context.
assertEquals(0, function () { return {x: 0} || this }().x);

// An object literal in test/value context.
assertEquals(x, function () { return {x: 0} && this }().x);

// An array literal in value/test context.
assertEquals(0, function () { return [0,1] || new Array(x,1) }()[0]);

// An array literal in test/value context.
assertEquals(x, function () { return [0,1] && new Array(x,1) }()[0]);

// Slot assignment in value/test context.
assertEquals(x, function (y) { return (y = 0) || x }("?"));
assertEquals(1, function (y) { return (y = 1) || x }("?"));

// Slot assignment in test/value context.
assertEquals(0, function (y) { return (y = 0) && x }("?"));
assertEquals(x, function (y) { return (y = 1) && x }("?"));

// void in value context.
assertEquals(void 0, function () { return void x }());

// void in value/test context.
assertEquals(x, function () { return (void x) || x }());

// void in test/value context.
assertEquals(void 0, function () { return (void x) && x }());

// Unary not in value context.
assertEquals(false, function () { return !x }());

// Unary not in value/test context.
assertEquals(true, function (y) { return !y || x }(0));
assertEquals(x, function (y) { return !y || x }(1));

// Unary not in test/value context.
assertEquals(x, function (y) { return !y && x }(0));
assertEquals(false, function (y) { return !y && x }(1));

// Comparison in value context.
assertEquals(false, function () { return x < x; }());

// Comparison in value/test context.
assertEquals(x, function () { return x < x || x; }());
assertEquals(true, function () { return x <= x || x; }());

// Comparison in test/value context.
assertEquals(false, function () { return x < x && x; }());
assertEquals(x, function () { return x <= x && x; }());
