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

// Test that boilerplate objects for array literals with non-constant
// elements (which will contain the hole at non-constant positions) will
// not cause prototype chain lookups when generating optimized code.

function f(x) {
  return [x][0];
}

// Test data element on prototype.
Object.prototype[0] = 23;
assertSame(1, f(1));
assertSame(2, f(2));
%OptimizeFunctionOnNextCall(f);
assertSame(3, f(3));
%DeoptimizeFunction(f);

// Test accessor element on prototype.
Object.prototype.__defineGetter__(0, function() { throw Error(); });
assertSame(4, f(4));
assertSame(5, f(5));
%OptimizeFunctionOnNextCall(f);
assertSame(6, f(6));
%DeoptimizeFunction(f);

// Test the same on boilerplate objects for object literals that contain
// both non-constant properties and non-constant elements.

function g(x, y) {
  var o = { foo:x, 0:y };
  return o.foo + o[0];
}

// Test data property and element on prototype.
Object.prototype[0] = 23;
Object.prototype.foo = 42;
assertSame(3, g(1, 2));
assertSame(5, g(2, 3));
%OptimizeFunctionOnNextCall(g);
assertSame(7, g(3, 4));
%DeoptimizeFunction(g);

// Test accessor property and element on prototype.
Object.prototype.__defineGetter__(0, function() { throw Error(); });
Object.prototype.__defineGetter__('foo', function() { throw Error(); });
assertSame(3, g(1, 2));
assertSame(5, g(2, 3));
%OptimizeFunctionOnNextCall(g);
assertSame(7, g(3, 4));
%DeoptimizeFunction(g);
