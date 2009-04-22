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

// Tests global loads from eval inside of a with statement.

var x = 27;

function test(obj, source) {
  with (obj) {
    eval(source);
  }
}

// Test shadowing in eval scope.
test({ x: 42 }, "assertEquals(42, x)");
test({ y: 42 }, "assertEquals(27, x)");

// Test shadowing in local scope inside an eval scope.
test({ x: 42 }, "function f() { assertEquals(42, x) }; f();");
test({ y: 42 }, "function f() { assertEquals(27, x) }; f();");

// Test shadowing in local scope inside an eval scope.  Deeper nesting
// this time.
test({ x: 42 }, "function f() { function g() { assertEquals(42, x) }; g() }; f();");
test({ y: 42 }, "function f() { function g() { assertEquals(27, x) }; g() }; f();");

// Test shadowing in local scope inside an eval scope with eval calls in the eval scopes.
test({ x: 42 }, "function f() { eval('1'); assertEquals(42, x) }; f();");
test({ y: 42 }, "function f() { eval('1'); assertEquals(27, x) }; f();");

// Test shadowing in local scope inside an eval scope with eval calls
// in the eval scopes.  Deeper nesting this time.
test({ x: 42 }, "function f() { function g() { eval('1'); assertEquals(42, x) }; g() }; f();");
test({ y: 42 }, "function f() { function g() { eval('1'); assertEquals(27, x) }; g() }; f();");

