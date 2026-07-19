// Copyright 2010 the V8 project authors. All rights reserved.
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

function props(x) {
  var result = [];
  for (var p in x) result.push(p);
  return result;
}

function A() {
  this.a1 = 1234;
  this.a2 = "D";
  this.a3 = false;
}

function B() {
  this.b3 = false;
  this.b2 = "D";
  this.b1 = 1234;
}

function C() {
  this.c3 = false;
  this.c1 = 1234;
  this.c2 = "D";
}

assertArrayEquals(["a1", "a2", "a3"], props(new A()));
assertArrayEquals(["b3", "b2", "b1"], props(new B()));
assertArrayEquals(["c3", "c1", "c2"], props(new C()));
assertArrayEquals(["s1", "s2", "s3"], props({s1: 0, s2: 0, s3: 0}));
assertArrayEquals(["s3", "s2", "s1"], props({s3: 0, s2: 0, s1: 0}));
assertArrayEquals(["s3", "s1", "s2"], props({s3: 0, s1: 0, s2: 0}));

var a = new A()
a.a0 = 0;
a.a4 = 0;
assertArrayEquals(["a1", "a2", "a3", "a0", "a4"], props(a));

var b = new B()
b.b4 = 0;
b.b0 = 0;
assertArrayEquals(["b3", "b2", "b1", "b4", "b0"], props(b));

var o1 = {s1: 0, s2: 0, s3: 0}
o1.s0 = 0;
o1.s4 = 0;
assertArrayEquals(["s1", "s2", "s3", "s0", "s4"], props(o1));

var o2 = {s3: 0, s2: 0, s1: 0}
o2.s4 = 0;
o2.s0 = 0;
assertArrayEquals(["s3", "s2", "s1", "s4", "s0"], props(o2));
