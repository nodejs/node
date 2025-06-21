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

// Test that we can inline a call with a parameter.
function TestInlineOneParam(o, p) {
  // Effect context.
  o.f(p);
  // Value context.
  var x = o.f(p);
  assertEquals(42, x);
  assertEquals(42, o.f(p));
  // Test context.
  if (!o.f(p)) {
    assertTrue(false);  // Should not happen.
  }
}

%PrepareFunctionForOptimization(TestInlineOneParam);
var obj = {x:42};
var o1 = {};
o1.f = function(o) { return o.x; };
for (var i = 0; i < 5; i++) TestInlineOneParam(o1, obj);
%OptimizeFunctionOnNextCall(TestInlineOneParam);
TestInlineOneParam(o1, obj);
TestInlineOneParam({f: o1.f}, {x:42});


function TestInlineTwoParams(o, p) {
  var y = 43;
  // Effect context.
  o.h(y, y);
  // Value context.
  var x = o.h(p, y);
  assertEquals(true, x);
  assertEquals(false, o.h(y, p));
  // Test context.
  if (!o.h(p, y)) {
    assertTrue(false);  // Should not happen.
  }

  // Perform the same tests again, but this time with non-trivial
  // expressions as the parameters.

  // Effect context.
  o.h(y + 1, y + 1);
  // Value context.
  var x = o.h(p + 1, y + 1);
  assertEquals(true, x);
  assertEquals(false, o.h(y + 1, p + 1));
  // Test context.
  if (!o.h(p + 1, y + 1)) {
    assertTrue(false);  // Should not happen.
  }
}

%PrepareFunctionForOptimization(TestInlineTwoParams);
var o2 = {};
o2.h = function(i, j) { return i < j; };
for (var i = 0; i < 5; i++) TestInlineTwoParams(o2, 42);
%OptimizeFunctionOnNextCall(TestInlineTwoParams);
TestInlineTwoParams(o2, 42);
TestInlineTwoParams({h: o2.h}, 42);
