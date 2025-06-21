// Copyright 2010 the V8 project authors. All rights reserved.
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

// Regression test for bugs when deoptimizing after assignments in effect
// contexts.

// Bug 989 is that there was an extra value on the expression stack when
// deoptimizing after an assignment in effect context (the value of the
// assignment was lingering).  This is hard to observe in the unoptimized
// code.
//
// This test uses comma expressions to put assignments in effect contexts,
// references to deleted global variables to force deoptimization, and
// function calls to observe an extra value.

function first(x, y) { return x; }
var y = 0;
var o = {};
o.x = 0;
o[0] = 0;

// Assignment to global variable.
x0 = 0;
function test0() { return first((y = 1, typeof x0), 2); }
// Call the function once to compile it.
assertEquals('number', test0());
// Delete to force deoptimization on the next call.
delete x0;
assertEquals('undefined', test0());

// Compound assignment to global variable.
x1 = 0;
function test1() { return first((y += 1, typeof x1), 2); }
assertEquals('number', test1(), 'test1 before');
delete x1;
assertEquals('undefined', test1(), 'test1 after');

// Pre and post-increment of global variable.
x2 = 0;
function test2() { return first((++y, typeof x2), 2); }
assertEquals('number', test2(), 'test2 before');
delete x2;
assertEquals('undefined', test2(), 'test2 after');

x3 = 0;
function test3() { return first((y++, typeof x3), 2); }
assertEquals('number', test3(), 'test3 before');
delete x3;
assertEquals('undefined', test3(), 'test3 after');


// Assignment, compound assignment, and pre and post-increment of named
// properties.
x4 = 0;
function test4() { return first((o.x = 1, typeof x4), 2); }
assertEquals('number', test4());
delete x4;
assertEquals('undefined', test4());

x5 = 0;
function test5() { return first((o.x += 1, typeof x5), 2); }
assertEquals('number', test5());
delete x5;
assertEquals('undefined', test5());

x6 = 0;
function test6() { return first((++o.x, typeof x6), 2); }
assertEquals('number', test6());
delete x6;
assertEquals('undefined', test6());

x7 = 0;
function test7() { return first((o.x++, typeof x7), 2); }
assertEquals('number', test7());
delete x7;
assertEquals('undefined', test7());


// Assignment, compound assignment, and pre and post-increment of indexed
// properties.
x8 = 0;
function test8(index) { return first((o[index] = 1, typeof x8), 2); }
assertEquals('number', test8());
delete x8;
assertEquals('undefined', test8());

x9 = 0;
function test9(index) { return first((o[index] += 1, typeof x9), 2); }
assertEquals('number', test9());
delete x9;
assertEquals('undefined', test9());

x10 = 0;
function test10(index) { return first((++o[index], typeof x10), 2); }
assertEquals('number', test10());
delete x10;
assertEquals('undefined', test10());

x11 = 0;
function test11(index) { return first((o[index]++, typeof x11), 2); }
assertEquals('number', test11());
delete x11;
assertEquals('undefined', test11());
