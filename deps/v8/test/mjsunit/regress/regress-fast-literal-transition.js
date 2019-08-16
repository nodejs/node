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

// Flags: --allow-natives-syntax --always-opt --expose-gc

// Test that the elements kind of the boilerplate object is sufficiently
// checked in LFastLiteral, so that unoptimized code can transition the
// boilerplate. The --always-opt flag makes sure that optimized code is
// not thrown away at deoptimization.

// The switch statement in f() makes sure that f() is not inlined. If we
// start inlining switch statements, we will still catch the bug on the
// final --stress-opt run.

function f(x) {
  switch (x) {
    case 1:
      return 1.4;
    case 2:
      return 1.5;
    case 3:
      return {};
    default:
      gc();
  }
}

function g(x) {
  return [1.1, 1.2, 1.3, f(x)];
}

// Step 1: Optimize g() to contain a PACKED_DOUBLE_ELEMENTS boilerplate.
;
%PrepareFunctionForOptimization(g);
assertEquals([1.1, 1.2, 1.3, 1.4], g(1));
assertEquals([1.1, 1.2, 1.3, 1.5], g(2));
%OptimizeFunctionOnNextCall(g);

// Step 2: Deoptimize g() and transition to PACKED_ELEMENTS boilerplate.
assertEquals([1.1, 1.2, 1.3, {}], g(3));

// Step 3: Cause a GC while broken clone of boilerplate is on the heap,
// hence causing heap verification to catch it.
assertEquals([1.1, 1.2, 1.3, undefined], g(4));
