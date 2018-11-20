// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//   * Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above
//     copyright notice, this list of conditions and the following
//     disclaimer in the documentation and/or other materials provided
//     with the distribution.
//   * Neither the name of Google Inc. nor the names of its
//     contributors may be used to endorse or promote products derived
//     from this software without specific prior written permission.
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

// Flags: --allow-natives-syntax --noalways-opt

var calls = 0;

function callsFReceiver(o) {
    return [].f.call(new Number(o.m), 1, 2, 3);
}

// For the HConstant
Array.prototype.f = function() {
    calls++;
    return +this;
};


var o1 = {m: 1};
var o2 = {a: 0, m:1};

var r1 = callsFReceiver(o1);
callsFReceiver(o1);
%OptimizeFunctionOnNextCall(callsFReceiver);
var r2 = callsFReceiver(o1);
assertOptimized(callsFReceiver);
callsFReceiver(o2);
assertUnoptimized(callsFReceiver);
var r3 = callsFReceiver(o1);

assertEquals(1, r1);
assertTrue(r1 === r2);
assertTrue(r2 === r3);

r1 = callsFReceiver(o1);
callsFReceiver(o1);
%OptimizeFunctionOnNextCall(callsFReceiver);
r2 = callsFReceiver(o1);
callsFReceiver(o2);
r3 = callsFReceiver(o1);

assertEquals(1, r1);
assertTrue(r1 === r2);
assertTrue(r2 === r3);

assertEquals(10, calls);
