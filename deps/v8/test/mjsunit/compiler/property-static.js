// Copyright 2013 the V8 project authors. All rights reserved.
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

// Test usage of static type information for loads that would otherwise
// turn into polymorphic or generic loads.

// Prepare a highly polymorphic load to be used by all tests.
Object.prototype.load = function() { return this.property; };
Object.prototype.load.call({ A:0, property:10 });
Object.prototype.load.call({ A:0, B:0, property:11 });
Object.prototype.load.call({ A:0, B:0, C:0, property:12 });
Object.prototype.load.call({ A:0, B:0, C:0, D:0, property:13 });
Object.prototype.load.call({ A:0, B:0, C:0, D:0, E:0, property:14 });
Object.prototype.load.call({ A:0, B:0, C:0, D:0, E:0, F:0, property:15 });

// Test for object literals.
(function() {
  function f(x) {
    var object = { property:x };
    return object.load();
  }

  assertSame(1, f(1));
  assertSame(2, f(2));
  %OptimizeFunctionOnNextCall(f);
  assertSame(3, f(3));
})();

// Test for inlined constructors.
(function() {
  function c(x) {
    this.property = x;
  }
  function f(x) {
    var object = new c(x);
    return object.load();
  }

  assertSame(1, f(1));
  assertSame(2, f(2));
  %OptimizeFunctionOnNextCall(f);
  assertSame(3, f(3));
})();
