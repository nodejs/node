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

// Flags: --allow-natives-syntax --opt

// The original problem from the bug: In the example below SMI check for b
// generated for inlining of equals invocation (marked with (*)) will be hoisted
// out of the loop across the typeof b === "object" condition and cause an
// immediate deopt. Another problem here is that no matter how many time we
// deopt and reopt we will continue to produce the wrong code.
//
// The fix is to notice when a deopt and subsequent reopt doesn't find
// additional type information, indicating that optimistic LICM should be
// disabled during compilation.

function eq(a, b) {
  if (typeof b === "object") {
    return b.equals(a);  // (*)
  }
  return a === b;
}

Object.prototype.equals = function (other) {
  return (this === other);
};

function test() {
  for (var i = 0; !eq(i, 10); i++)
    ;
}

%PrepareFunctionForOptimization(test);
eq({}, {});
eq({}, {});
eq(1, 1);
eq(1, 1);
test();
%OptimizeFunctionOnNextCall(test);
test();
%PrepareFunctionForOptimization(test);
%OptimizeFunctionOnNextCall(test);
// Second compilation should have noticed that LICM wasn't a good idea, and now
// function should no longer deopt when called.
test();
assertOptimized(test);
