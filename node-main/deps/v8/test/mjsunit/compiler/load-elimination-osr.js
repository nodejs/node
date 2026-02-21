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

// Flags: --allow-natives-syntax --load-elimination

// Test global load elimination in the presence of OSR compilation.

function It(x, y) { }

function foo_osr(x, limit) {
  var o = new It();
  o.x = x;
  o.y = x;
  for (var i = 0; i < limit; i++) {
    o.y += o.x;  // Load of x cannot be hoisted due to OSR.
  }

  return o.y;
}

assertEquals(22, foo_osr(11, 1));
assertEquals(24, foo_osr(12, 1));
assertEquals(1300013, foo_osr(13, 100000));


function foo_hot(x, limit) {
  var o = new It();
  o.x = x;
  o.y = x;
  for (var i = 0; i < limit; i++) {
    o.y += o.x;  // Load of x can be hoisted without OSR.
  }

  return o.y;
}

%PrepareFunctionForOptimization(foo_hot);
assertEquals(22, foo_hot(11, 1));
assertEquals(24, foo_hot(12, 1));
%OptimizeFunctionOnNextCall(foo_hot);
assertEquals(32, foo_hot(16, 1));
assertEquals(1300013, foo_hot(13, 100000));
