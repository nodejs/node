// Copyright 2008 the V8 project authors. All rights reserved.
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

// Allocate a very large object that is guaranteed to overflow the
// instance_size field in the map resulting in an object that is smaller
// than what was called for.
function LargeObject(i) {
  this.a = i;
  this.b = i;
  this.c = i;
  this.d = i;
  this.e = i;
  this.f = i;
  this.g = i;
  this.h = i;
  this.i = i;
  this.j = i;
  this.k = i;
  this.l = i;
  this.m = i;
  this.n = i;
  this.o = i;
  this.p = i;
  this.q = i;
  this.r = i;
  this.s = i;
  this.t = i;
  this.u = i;
  this.v = i;
  this.w = i;
  this.x = i;
  this.y = i;
  this.z = i;
  this.a1 = i;
  this.b1 = i;
  this.c1 = i;
  this.d1 = i;
  this.e1 = i;
  this.f1 = i;
  this.g1 = i;
  this.h1 = i;
  this.i1 = i;
  this.j1 = i;
  this.k1 = i;
  this.l1 = i;
  this.m1 = i;
  this.n1 = i;
  this.o1 = i;
  this.p1 = i;
  this.q1 = i;
  this.r1 = i;
  this.s1 = i;
  this.t1 = i;
  this.u1 = i;
  this.v1 = i;
  this.w1 = i;
  this.x1 = i;
  this.y1 = i;
  this.z1 = i;
  this.a2 = i;
  this.b2 = i;
  this.c2 = i;
  this.d2 = i;
  this.e2 = i;
  this.f2 = i;
  this.g2 = i;
  this.h2 = i;
  this.i2 = i;
  this.j2 = i;
  this.k2 = i;
  this.l2 = i;
  this.m2 = i;
  this.n2 = i;
  this.o2 = i;
  this.p2 = i;
  this.q2 = i;
  this.r2 = i;
  this.s2 = i;
  this.t2 = i;
  this.u2 = i;
  this.v2 = i;
  this.w2 = i;
  this.x2 = i;
  this.y2 = i;
  this.z2 = i;
  this.a3 = i;
  this.b3 = i;
  this.c3 = i;
  this.d3 = i;
  this.e3 = i;
  this.f3 = i;
  this.g3 = i;
  this.h3 = i;
  this.i3 = i;
  this.j3 = i;
  this.k3 = i;
  this.l3 = i;
  this.m3 = i;
  this.n3 = i;
  this.o3 = i;
  this.p3 = i;
  this.q3 = i;
  this.r3 = i;
  this.s3 = i;
  this.t3 = i;
  this.u3 = i;
  this.v3 = i;
  this.w3 = i;
  this.x3 = i;
  this.y3 = i;
  this.z3 = i;
  this.a4 = i;
  this.b4 = i;
  this.c4 = i;
  this.d4 = i;
  this.e4 = i;
  this.f4 = i;
  this.g4 = i;
  this.h4 = i;
  this.i4 = i;
  this.j4 = i;
  this.k4 = i;
  this.l4 = i;
  this.m4 = i;
  this.n4 = i;
  this.o4 = i;
  this.p4 = i;
  this.q4 = i;
  this.r4 = i;
  this.s4 = i;
  this.t4 = i;
  this.u4 = i;
  this.v4 = i;
  this.w4 = i;
  this.x4 = i;
  this.y4 = i;
  this.z4 = i;
  this.a5 = i;
  this.b5 = i;
  this.c5 = i;
  this.d5 = i;
  this.e5 = i;
  this.f5 = i;
  this.g5 = i;
  this.h5 = i;
  this.i5 = i;
  this.j5 = i;
  this.k5 = i;
  this.l5 = i;
  this.m5 = i;
  this.n5 = i;
  this.o5 = i;
  this.p5 = i;
  this.q5 = i;
  this.r5 = i;
  this.s5 = i;
  this.t5 = i;
  this.u5 = i;
  this.v5 = i;
  this.w5 = i;
  this.x5 = i;
  this.y5 = i;
  this.z5 = i;
  this.a6 = i;
  this.b6 = i;
  this.c6 = i;
  this.d6 = i;
  this.e6 = i;
  this.f6 = i;
  this.g6 = i;
  this.h6 = i;
  this.i6 = i;
  this.j6 = i;
  this.k6 = i;
  this.l6 = i;
  this.m6 = i;
  this.n6 = i;
  this.o6 = i;
  this.p6 = i;
  this.q6 = i;
  this.r6 = i;
  this.s6 = i;
  this.t6 = i;
  this.u6 = i;
  this.v6 = i;
  this.w6 = i;
  this.x6 = i;
  this.y6 = i;
  this.z6 = i;
  this.a7 = i;
  this.b7 = i;
  this.c7 = i;
  this.d7 = i;
  this.e7 = i;
  this.f7 = i;
  this.g7 = i;
  this.h7 = i;
  this.i7 = i;
  this.j7 = i;
  this.k7 = i;
  this.l7 = i;
  this.m7 = i;
  this.n7 = i;
  this.o7 = i;
  this.p7 = i;
  this.q7 = i;
  this.r7 = i;
  this.s7 = i;
  this.t7 = i;
  this.u7 = i;
  this.v7 = i;
  this.w7 = i;
  this.x7 = i;
  this.y7 = i;
  this.z7 = i;
  this.a8 = i;
  this.b8 = i;
  this.c8 = i;
  this.d8 = i;
  this.e8 = i;
  this.f8 = i;
  this.g8 = i;
  this.h8 = i;
  this.i8 = i;
  this.j8 = i;
  this.k8 = i;
  this.l8 = i;
  this.m8 = i;
  this.n8 = i;
  this.o8 = i;
  this.p8 = i;
  this.q8 = i;
  this.r8 = i;
  this.s8 = i;
  this.t8 = i;
  this.u8 = i;
  this.v8 = i;
  this.w8 = i;
  this.x8 = i;
  this.y8 = i;
  this.z8 = i;
  this.a9 = i;
  this.b9 = i;
  this.c9 = i;
  this.d9 = i;
  this.e9 = i;
  this.f9 = i;
  this.g9 = i;
  this.h9 = i;
  this.i9 = i;
  this.j9 = i;
  this.k9 = i;
  this.l9 = i;
  this.m9 = i;
  this.n9 = i;
  this.o9 = i;
  this.p9 = i;
  this.q9 = i;
  // With this number of properties the object perfectly wraps around if the
  // instance size is not checked when allocating the initial map for MultiProp.
  // Meaning that the instance will be smaller than a minimal JSObject and we
  // will suffer a bus error in the release build or an assertion in the debug
  // build.
}

function ExpectAllFields(o, val) {
  for (var x in o) {
    assertEquals(o[x], val);
  }
}

var a = new LargeObject(1);
var b = new LargeObject(2);

ExpectAllFields(a, 1);
ExpectAllFields(b, 2);
