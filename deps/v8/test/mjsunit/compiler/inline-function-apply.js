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

// Test inlining and deoptimization of function.apply(this, arguments)
// calls for which the exact number of arguments is known.
(function () {
  "use strict";
  function test(argumentsCount) {
    var dispatcher = {};
    var deoptimize = { deopt:false };
    dispatcher["const" + argumentsCount] = 0;
    dispatcher.func = C;

    function A(x,y) {
      var r = "A";
      if (argumentsCount == 1) r += B(10);
      if (argumentsCount == 2) r += B(10, 11);
      if (argumentsCount == 3) r += B(10, 11, 12);
      assertSame(1, x);
      assertSame(2, y);
      return r;
    }

    function B(x,y) {
      x = 0; y = 0;
      var r = "B" + dispatcher.func.apply(this, arguments);
      assertSame(argumentsCount, arguments.length);
      for (var i = 0; i < arguments.length; i++) {
        assertSame(10 + i, arguments[i]);
      }
      return r;
    }

    function C(x,y) {
      x = 0; y = 0;
      var r = "C"
      deoptimize.deopt;
      assertSame(argumentsCount, arguments.length);
      for (var i = 0; i < arguments.length; i++) {
        assertSame(10 + i, arguments[i]);
      }
      return r;
    }

    assertEquals("ABC", A(1,2));
    assertEquals("ABC", A(1,2));
    %OptimizeFunctionOnNextCall(A);
    assertEquals("ABC", A(1,2));
    delete deoptimize.deopt;
    assertEquals("ABC", A(1,2));

    %DeoptimizeFunction(A);
    %ClearFunctionTypeFeedback(A);
    %DeoptimizeFunction(B);
    %ClearFunctionTypeFeedback(B);
    %DeoptimizeFunction(C);
    %ClearFunctionTypeFeedback(C);
  }

  for (var a = 1; a <= 3; a++) {
    test(a);
  }
})();
