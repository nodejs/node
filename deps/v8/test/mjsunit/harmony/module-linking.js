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

// Flags: --harmony-modules --harmony-scoping

// Test basic module linking.

"use strict";

let log = "";

export let x = (log += "1");

export module B = A.B

export module A {
  export let x = (log += "2");
  let y = (log += "3");
  export function f() { log += "5" };
  export module B {
    module BB = B;
    export BB, x;
    let x = (log += "4");
    f();
    let y = (log += "6");
  }
  export let z = (log += "7");
  export module C {
    export let z = (log += "8");
    export module D = B
    export module C = A.C
  }
  module D {}
}

export module M1 {
  export module A2 = M2;
  export let x = (log += "9");
}
export module M2 {
  export module A1 = M1;
  export let x = (log += "0");
}

assertEquals("object", typeof A);
assertTrue('x' in A);
assertFalse('y' in A);
assertTrue('f' in A);
assertTrue('B' in A);
assertTrue('z' in A);
assertTrue('C' in A);
assertFalse('D' in A);

assertEquals("object", typeof B);
assertTrue('BB' in B);
assertTrue('x' in B);
assertFalse('y' in B);

assertEquals("object", typeof A.B);
assertTrue('BB' in A.B);
assertTrue('x' in A.B);
assertFalse('y' in A.B);

assertEquals("object", typeof A.B.BB);
assertTrue('BB' in A.B.BB);
assertTrue('x' in A.B.BB);
assertFalse('y' in A.B.BB);

assertEquals("object", typeof A.C);
assertTrue('z' in A.C);
assertTrue('D' in A.C);
assertTrue('C' in A.C);

assertEquals("object", typeof M1);
assertEquals("object", typeof M2);
assertTrue('A2' in M1);
assertTrue('A1' in M2);
assertEquals("object", typeof M1.A2);
assertEquals("object", typeof M2.A1);
assertTrue('A1' in M1.A2);
assertTrue('A2' in M2.A1);
assertEquals("object", typeof M1.A2.A1);
assertEquals("object", typeof M2.A1.A2);

assertSame(B, A.B);
assertSame(B, B.BB);
assertSame(B, A.C.D);
assertSame(A.C, A.C.C);
assertFalse(A.D === A.C.D);

assertSame(M1, M2.A1);
assertSame(M2, M1.A2);
assertSame(M1, M1.A2.A1);
assertSame(M2, M2.A1.A2);

// TODO(rossberg): inner declarations are not executed yet.
// assertEquals("1234567890", log);
