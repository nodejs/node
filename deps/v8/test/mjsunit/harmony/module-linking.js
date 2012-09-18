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

// Test basic module linking and initialization.

"use strict";

module R {
  // At this point, only functions and modules are initialized.
  assertEquals(undefined, v)
  assertEquals(undefined, vv)
  assertEquals(undefined, R.v)
  assertEquals(undefined, M.v)
  assertEquals(undefined, MM.v)
  assertEquals(undefined, F.v)
  assertEquals(undefined, G.v)
  assertThrows(function() { l }, ReferenceError)
  assertThrows(function() { ll }, ReferenceError)
  assertThrows(function() { R.l }, ReferenceError)
  assertThrows(function() { M.l }, ReferenceError)
  assertThrows(function() { MM.l }, ReferenceError)
  assertThrows(function() { F.l }, ReferenceError)
  assertThrows(function() { G.l }, ReferenceError)
  assertThrows(function() { c }, ReferenceError)
  assertThrows(function() { cc }, ReferenceError)
  assertThrows(function() { R.c }, ReferenceError)
  assertThrows(function() { M.c }, ReferenceError)
  assertThrows(function() { MM.c }, ReferenceError)
  assertThrows(function() { F.c }, ReferenceError)
  assertThrows(function() { G.c }, ReferenceError)
  assertEquals(4, f())
  assertEquals(24, ff())
  assertEquals(4, R.f())
  assertEquals(14, M.f())
  assertEquals(34, MM.f())
  assertEquals(44, F.f())
  assertEquals(14, G.f())

  // All properties should already exist on the instance objects, though.
  assertTrue("v" in R)
  assertTrue("v" in RR)
  assertTrue("v" in M)
  assertTrue("v" in MM)
  assertTrue("v" in F)
  assertTrue("v" in G)
  assertTrue("l" in R)
  assertTrue("l" in RR)
  assertTrue("l" in M)
  assertTrue("l" in MM)
  assertTrue("l" in F)
  assertTrue("l" in G)
  assertTrue("c" in R)
  assertTrue("c" in RR)
  assertTrue("c" in M)
  assertTrue("c" in MM)
  assertTrue("c" in F)
  assertTrue("c" in G)
  assertTrue("f" in R)
  assertTrue("f" in RR)
  assertTrue("f" in M)
  assertTrue("f" in MM)
  assertTrue("f" in F)
  assertTrue("f" in G)
  assertTrue("M" in R)
  assertTrue("M" in RR)
  assertTrue("RR" in R)
  assertTrue("RR" in RR)

  // And aliases should be identical.
  assertSame(R, RR)
  assertSame(R, R.RR)
  assertSame(M, R.M)
  assertSame(M, G)

  // We can only assign to var.
  assertEquals(-1, v = -1)
  assertEquals(-2, R.v = -2)
  assertEquals(-2, v)
  assertEquals(-2, R.v)

  assertThrows(function() { l = -1 }, ReferenceError)
  assertThrows(function() { R.l = -2 }, ReferenceError)
  assertThrows(function() { l }, ReferenceError)
  assertThrows(function() { R.l }, ReferenceError)

  assertThrows(function() { eval("c = -1") }, SyntaxError)
  assertThrows(function() { R.c = -2 }, TypeError)

  // Initialize first bunch or variables.
  export var v = 1
  export let l = 2
  export const c = 3
  export function f() { return 4 }

  assertEquals(1, v)
  assertEquals(1, R.v)
  assertEquals(2, l)
  assertEquals(2, R.l)
  assertEquals(3, c)
  assertEquals(3, R.c)

  assertEquals(-3, v = -3)
  assertEquals(-4, R.v = -4)
  assertEquals(-3, l = -3)
  assertEquals(-4, R.l = -4)
  assertThrows(function() { eval("c = -3") }, SyntaxError)
  assertThrows(function() { R.c = -4 }, TypeError)

  assertEquals(-4, v)
  assertEquals(-4, R.v)
  assertEquals(-4, l)
  assertEquals(-4, R.l)
  assertEquals(3, c)
  assertEquals(3, R.c)

  // Initialize nested module.
  export module M {
    export var v = 11
    export let l = 12
    export const c = 13
    export function f() { return 14 }
  }

  assertEquals(11, M.v)
  assertEquals(11, G.v)
  assertEquals(12, M.l)
  assertEquals(12, G.l)
  assertEquals(13, M.c)
  assertEquals(13, G.c)

  // Initialize non-exported variables.
  var vv = 21
  let ll = 22
  const cc = 23
  function ff() { return 24 }

  assertEquals(21, vv)
  assertEquals(22, ll)
  assertEquals(23, cc)

  // Initialize non-exported module.
  module MM {
    export var v = 31
    export let l = 32
    export const c = 33
    export function f() { return 34 }
  }

  assertEquals(31, MM.v)
  assertEquals(32, MM.l)
  assertEquals(33, MM.c)

  // Recursive self reference.
  export module RR = R
}

// Initialize sibling module that was forward-used.
module F {
  assertEquals(undefined, v)
  assertEquals(undefined, F.v)
  assertThrows(function() { l }, ReferenceError)
  assertThrows(function() { F.l }, ReferenceError)
  assertThrows(function() { c }, ReferenceError)
  assertThrows(function() { F.c }, ReferenceError)

  export var v = 41
  export let l = 42
  export const c = 43
  export function f() { return 44 }

  assertEquals(41, v)
  assertEquals(41, F.v)
  assertEquals(42, l)
  assertEquals(42, F.l)
  assertEquals(43, c)
  assertEquals(43, F.c)
}

// Define recursive module alias.
module G = R.M



// Second test with side effects and more module nesting.

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

assertEquals("1234567890", log);
