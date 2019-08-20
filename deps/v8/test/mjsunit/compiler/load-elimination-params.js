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

// Test local load elimination of redundant loads and stores.

function B(x, y) {
  this.x = x;
  this.y = y;
  return this;
}

function test_params1(a, b) {
  var i = a.x;
  var j = a.x;
  var k = b.x;
  var l = b.x;
  return i + j + k + l;
}

%PrepareFunctionForOptimization(test_params1);

assertEquals(14, test_params1(new B(3, 4), new B(4, 5)));
assertEquals(110, test_params1(new B(11, 7), new B(44, 8)));

%OptimizeFunctionOnNextCall(test_params1);

assertEquals(6, test_params1(new B(1, 7), new B(2, 8)));

function test_params2(a, b) {
  var o = new B(a + 1, b);
  o.x = a;
  var i = o.x;
  o.x = a;
  var j = o.x;
  o.x = b;
  var k = o.x;
  o.x = b;
  var l = o.x;
  return i + j + k + l;
}

%PrepareFunctionForOptimization(test_params2);

assertEquals(14, test_params2(3, 4));
assertEquals(110, test_params2(11, 44));

%OptimizeFunctionOnNextCall(test_params2);

assertEquals(6, test_params2(1, 2));
