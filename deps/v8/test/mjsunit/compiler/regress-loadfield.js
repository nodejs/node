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

// Flags: --allow-natives-syntax

// Regression test for GVN on field loads.

function bar() {}

// Make sure there is a transition on adding "bar" inobject property.
var b = new bar();
b.bar = "bar";

function test(a) {
  var b = new Array(10);
  for (var i = 0; i < 10; i++) {
    b[i] = new bar();
  }

  for (var i = 0; i < 10; i++) {
    b[i].bar = a.foo;
  }
}

%PrepareFunctionForOptimization(test);

// Create an object with fast backing store properties.
var a = {};
a.p1 = "";
a.p2 = "";
a.p3 = "";
a.p4 = "";
a.p5 = "";
a.p6 = "";
a.p7 = "";
a.p8 = "";
a.p9 = "";
a.p10 = "";
a.p11 = "";
a.foo = "foo";
for (var i = 0; i < 5; i++) {
 test(a);
}
%OptimizeFunctionOnNextCall(test);
test(a);

test("");
