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

var f = Function();
assertTrue(typeof f() == 'undefined');
f = new Function();
assertTrue(typeof f() == 'undefined');

f = Function('return 1');
assertEquals(1, f());
f = new Function('return 1');
assertEquals(1, f());

f = Function('return true');
assertTrue(f());
f = new Function('return true');
assertTrue(f());

f = Function('x', 'return x');
assertEquals(1, f(1));
assertEquals('bar', f('bar'));
assertTrue(typeof f() == 'undefined');
var x = {};
assertTrue(x === f(x));

f = Function('x', 'return x // comment');
assertEquals(1, f(1));

f = Function('return typeof anonymous');
assertEquals('undefined', f());

var anonymous = 42;
f = Function('return anonymous;');
assertEquals(42, f());

f = new Function('x', 'return x')
assertEquals(1, f(1));
assertEquals('bar', f('bar'));
assertTrue(typeof f() == 'undefined');
var x = {};
assertTrue(x === f(x));

f = Function('x', 'y', 'return x+y');
assertEquals(5, f(2, 3));
assertEquals('foobar', f('foo', 'bar'));
f = new Function('x', 'y', 'return x+y');
assertEquals(5, f(2, 3));
assertEquals('foobar', f('foo', 'bar'));

var x = {}; x.toString = function() { return 'x'; };
var y = {}; y.toString = function() { return 'y'; };
var z = {}; z.toString = function() { return 'return x*y'; }
var f = Function(x, y, z);
assertEquals(25, f(5, 5));
assertEquals(42, f(2, 21));
f = new Function(x, y, z);
assertEquals(25, f(5, 5));
assertEquals(42, f(2, 21));

