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

// Test that we can inline a function that returns a constant.
function TestInlineConstant(o) {
  // Effect context.
  o.f();
  // Value context.
  var x = o.f();
  assertEquals(42, x);
  assertEquals(42, o.f());
  // Test context.
  if (!o.f()) {
    assertTrue(false);  // Should not happen.
  }
}

var o1 = {};
o1.f = function() { return 42; };
for (var i = 0; i < 10000; i++) TestInlineConstant(o1);
TestInlineConstant({f: o1.f});


// Test that we can inline a function that returns 'this'.
function TestInlineThis(o) {
  // Effect context.
  o.g();
  // Value context.
  var x = o.g();
  assertEquals(o, x);
  assertEquals(o, o.g());
  // Test context.
  if (!o.g()) {
    assertTrue(false);  // Should not happen.
  }
}

var o2 = {};
o2.g = function() { return this; };
for (var i = 0; i < 10000; i++) TestInlineThis(o2);
TestInlineThis({g: o2.g});


// Test that we can inline a function that returns 'this.x'.
function TestInlineThisX(o) {
  // Effect context.
  o.h();
  // Value context.
  var x = o.h();
  assertEquals(42, x);
  assertEquals(42, o.h());
  // Test context.
  if (!o.h()) {
    assertTrue(false);  // Should not happen.
  }
}

var o3 = {y:0,x:42};
o3.h = function() { return this.x; };
for (var i = 0; i < 10000; i++) TestInlineThisX(o3);
TestInlineThisX({h: o3.h, x:42});


// Test that we can inline a function that returns 'this.x.length'.
function TestInlineThisXLength(o) {
  // Effect context.
  o.h();
  // Value context.
  var x = o.h();
  assertEquals(3, x);
  assertEquals(3, o.h());
  // Test context.
  if (!o.h()) {
    assertTrue(false);  // Should not happen.
  }
}

var o4 = {x:[1,2,3]};
o4.h = function() { return this.x.length; };
for (var i = 0; i < 10000; i++) TestInlineThisXLength(o4);
TestInlineThisXLength({h: o4.h, x:[1,2,3]});


// Test that we can inline a function that returns 'this.x.y'.
function TestInlineThisXY(o) {
  // Effect context.
  o.h();
  // Value context.
  var x = o.h();
  assertEquals(42, x);
  assertEquals(42, o.h());
  // Test context.
  if (!o.h()) {
    assertTrue(false);  // Should not happen.
  }
}

var o6 = {y:42}
var o5 = {e:o6};
o5.h = function() { return this.e.y; };
for (var i = 0; i < 10000; i++) TestInlineThisXY(o5);
TestInlineThisXY({h: o5.h, e:o6});


// Test that we can inline a function that returns 'this.x.length'.
function TestInlineThisX0(o) {
  // Effect context.
  o.foo();
  // Value context.
  var x = o.foo();
  assertEquals(42, x);
  assertEquals(42, o.foo());
  // Test context.
  if (!o.foo()) {
    assertTrue(false);  // Should not happen.
  }
}

var o7 = {x:[42,43,44]};
o7.foo = function() { return this.x[0]; };
for (var i = 0; i < 10000; i++) TestInlineThisX0(o7);
TestInlineThisX0({foo: o7.foo, x:[42,0,0]});
