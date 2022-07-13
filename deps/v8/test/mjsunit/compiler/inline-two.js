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

// Flags: --allow-natives-syntax

// Test that we can inline a function that calls another function.
function TestInlineX(o) {
  // Effect context.
  o.g();
  // Value context.
  var x = o.g();
  assertEquals(42, x);
  assertEquals(42, o.g());
  // Test context.
  if (!o.g()) {
    assertTrue(false);  // Should not happen.
  }
}

%PrepareFunctionForOptimization(TestInlineX);
var o2 = {};
o2.size = function() { return 42; }
o2.g = function() { return this.size(); };
for (var i = 0; i < 5; i++) TestInlineX(o2);
%OptimizeFunctionOnNextCall(TestInlineX);
TestInlineX(o2);
TestInlineX({g: o2.g, size:o2.size});


// Test that we can inline a call on a non-variable receiver.
function TestInlineX2(o) {
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

%PrepareFunctionForOptimization(TestInlineX2);
var obj = {}
obj.foo = function() { return 42; }
var o3 = {};
o3.v = obj;
o3.h = function() { return this.v.foo(); };
for (var i = 0; i < 5; i++) TestInlineX2(o3);
%OptimizeFunctionOnNextCall(TestInlineX2);
TestInlineX2(o3);
TestInlineX2({h: o3.h, v:obj});


// Test that we can inline a call on a non-variable receiver.
function TestInlineFG(o) {
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

%PrepareFunctionForOptimization(TestInlineFG);
var obj = {}
obj.g = function() { return 42; }
var o3 = {};
o3.v = obj;
o3.f = function() { return this.v; }
o3.h = function() { return this.f().g(); };
for (var i = 0; i < 5; i++) TestInlineFG(o3);
%OptimizeFunctionOnNextCall(TestInlineFG);
TestInlineFG(o3);
TestInlineFG({h: o3.h, f: o3.f, v:obj});
