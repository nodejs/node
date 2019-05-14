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

// Flags: --allow-natives-syntax

function DoubleContainer() {
  this.x0 = 0.5;
  this.x1 = undefined;
  this.x2 = undefined;
  this.x3 = undefined;
  this.x4 = undefined;
  this.x5 = undefined;
  this.x6 = undefined;
  this.x7 = 5;
  this.x8 = undefined;
  this.x9 = undefined;
  this.x10 = undefined;
  this.x11 = undefined;
  this.x12 = undefined;
  this.x13 = undefined;
  this.x14 = undefined;
  this.x15 = undefined;
  this.x16 = true;
  this.y = 2.5;
}

var z = new DoubleContainer();

function test_props(a) {
  for (var i in a) {
    assertTrue(i !== "x0" || a[i] === 0.5);
    assertTrue(i !== "y" || a[i] === 2.5);
    assertTrue(i !== "x12" || a[i] === undefined);
    assertTrue(i !== "x16" || a[i] === true);
    assertTrue(i !== "x7" || a[i] === 5);
  }
}

test_props(z);
test_props(z);
%OptimizeFunctionOnNextCall(test_props);
test_props(z);
