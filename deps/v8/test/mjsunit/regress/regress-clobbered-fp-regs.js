// Copyright 2011 the V8 project authors. All rights reserved.
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
//
// Flags: --allow-natives-syntax

function store(a, x, y) {
  var f1 = 0.1 * y;
  var f2 = 0.2 * y;
  var f3 = 0.3 * y;
  var f4 = 0.4 * y;
  var f5 = 0.5 * y;
  var f6 = 0.6 * y;
  var f7 = 0.7 * y;
  var f8 = 0.8 * y;
  a[0] = x;
  var sum = f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8;
  assertEquals(1, y);
  var expected = 3.6;
  if (Math.abs(expected - sum) > 0.01) {
    assertEquals(expected, sum);
  }
}

// Generate TransitionElementsKindStub.
;
%PrepareFunctionForOptimization(store);
store([1], 1, 1);
store([1], 1.1, 1);
store([1], 1.1, 1);
%OptimizeFunctionOnNextCall(store);
// This will trap on allocation site in TransitionElementsKindStub.
store([1], 1, 1);
