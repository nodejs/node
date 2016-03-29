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

// Flags: --allow-natives-syntax --no-fold-constants

function add(x, y) {
  return x + y;
}

assertEquals(0, add(0, 0));
assertEquals(0, add(0, 0));
%OptimizeFunctionOnNextCall(add);
assertEquals(-0, add(-0, -0));


function testsin() {
  assertEquals(-0, Math.sin(-0));
}

testsin();
testsin();
%OptimizeFunctionOnNextCall(testsin);
testsin();


function testfloor() {
  assertEquals(-0, Math.floor(-0));
}

testfloor();
testfloor();
%OptimizeFunctionOnNextCall(testfloor);
testfloor();


var double_one = Math.cos(0);

function add(a, b) {
  return a + b;
}

assertEquals(1, 1/add(double_one, 0));
assertEquals(1, 1/add(0, double_one));
%OptimizeFunctionOnNextCall(add);
assertEquals(1/(-0 + -0), 1/add(-0, -0));
