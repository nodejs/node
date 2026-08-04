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

// Test materialization of an arguments object with unknown argument
// values in non-strict mode (length has to be zero).
(function() {
  var deoptimize = { deopt:true };
  var object = {};

  object.a = function A(x, y, z) {
    assertSame(0, arguments.length);
    return this.b();
  };

  object.b = function B() {
    assertSame(0, arguments.length);
    deoptimize.deopt;
    return arguments.length;
  };

  %PrepareFunctionForOptimization(object.a);
  assertSame(0, object.a());
  assertSame(0, object.a());
  %OptimizeFunctionOnNextCall(object.a);
  assertSame(0, object.a());
  delete deoptimize.deopt;
  assertSame(0, object.a());
})();


// Test materialization of an arguments object with unknown argument
// values in strict mode (length is allowed to exceed stack size).
(function() {
  'use strict';
  var deoptimize = { deopt:true };
  var object = {};

  object.a = function A(x, y, z) {
    assertSame(0, arguments.length);
    return this.b(1, 2, 3, 4, 5, 6, 7, 8);
  };

  object.b = function B(a, b, c, d, e, f, g, h) {
    assertSame(8, arguments.length);
    deoptimize.deopt;
    return arguments.length;
  };

  %PrepareFunctionForOptimization(object.a);
  assertSame(8, object.a());
  assertSame(8, object.a());
  %OptimizeFunctionOnNextCall(object.a);
  assertSame(8, object.a());
  delete deoptimize.deopt;
  assertSame(8, object.a());
})();
