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

// Flags: --allow-natives-syntax --opt --no-always-opt

function divp4(x) {
  return x / 4;
}

divp4(8);
divp4(8);
%OptimizeFunctionOnNextCall(divp4);
assertEquals(2, divp4(8));
assertEquals(0.5, divp4(2));


function divn4(x) {
  return x / (-4);
}

divn4(8);
divn4(8);
%OptimizeFunctionOnNextCall(divn4);
assertEquals(-2, divn4(8));
// Check for (0 / -x)
assertEquals(-0, divn4(0));


// Check for (kMinInt / -1)
function divn1(x) {
  return x / (-1);
}

var two_31 = 1 << 31;
divn1(2);
divn1(2);
%OptimizeFunctionOnNextCall(divn1);
assertEquals(-2, divn1(2));
assertEquals(-two_31, divn1(two_31));


//Check for truncating to int32 case
function divp4t(x) {
  return (x / 4) | 0;
}

divp4t(8);
divp4t(8);
%OptimizeFunctionOnNextCall(divp4t);
assertEquals(-1, divp4t(-5));
assertEquals(1, divp4t(5));
assertOptimized(divp4t);

function divn4t(x) {
  return (x / -4) | 0;
}

divn4t(8);
divn4t(8);
%OptimizeFunctionOnNextCall(divn4t);
assertEquals(1, divn4t(-5));
assertEquals(-1, divn4t(5));
assertOptimized(divn4t);

// Check kMinInt case.
function div_by_two(x) {
  return (x / 2) | 0;
}

div_by_two(12);
div_by_two(34);
%OptimizeFunctionOnNextCall(div_by_two);
div_by_two(56);
assertEquals(-(1 << 30), div_by_two(1 << 31));
