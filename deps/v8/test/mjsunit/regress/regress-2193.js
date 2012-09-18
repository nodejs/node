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

// Flags: --allow-natives-syntax --cache-optimized-code

function bozo() {};
function MakeClosure() {
  return function f(use_literals) {
    if (use_literals) {
      return [1,2,3,3,4,5,6,7,8,9,bozo];
    } else {
      return 0;
    }
  }
}

// Create two closures that share the same literal boilerplates.
var closure1 = MakeClosure();
var closure2 = MakeClosure();
var expected = [1,2,3,3,4,5,6,7,8,9,bozo];

// Make sure we generate optimized code for the first closure after
// warming it up properly so that the literals boilerplate is generated
// and the optimized code uses CreateArrayLiteralShallow runtime call.
assertEquals(0, closure1(false));
assertEquals(expected, closure1(true));
%OptimizeFunctionOnNextCall(closure1);
assertEquals(expected, closure1(true));

// Optimize the second closure, which should reuse the optimized code
// from the first closure with the same literal boilerplates.
assertEquals(0, closure2(false));
%OptimizeFunctionOnNextCall(closure2);
assertEquals(expected, closure2(true));
