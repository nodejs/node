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

// Flags: --harmony-modules

// Test basic module syntax, with and without automatic semicolon insertion.

module A {}

module A1 = A
module A2 = A;
module A3 = A2

module B {
  export vx
  export vy, lz, c, f

  var vx
  var vx, vy;
  var vx = 0, vy
  let lx, ly
  let lz = 1
  const c = 9
  function f() {}

  module C0 {}

  export module C {
    let x
    export module D { export let x }
    let y
  }

  let zz = ""

  export var x0
  export var x1, x2 = 6, x3
  export let y0
  export let y1 = 0, y2
  export const z0 = 0
  export const z1 = 2, z2 = 3
  export function f0() {}
  export module M1 {}
  export module M2 = C.D
  export module M3 at "http://where"

  import i0 from I
  import i1, i2, i3, M from I
  import i4, i5 from "http://where"
}

module I {
  export let i0, i1, i2, i3;
  export module M {}
}

module C1 = B.C;
module D1 = B.C.D
module D2 = C1.D
module D3 = D2

module E1 at "http://where"
module E2 at "http://where";
module E3 = E1.F

// Check that ASI does not interfere.

module X
{
let x
}

module Y
=
X

module Z
at
"file://local"

import
x
,
y
from
"file://local"


module Wrap {
export
x
,
y

export
var
v1 = 1

export
let
v2 = 2

export
const
v3 = 3

export
function
f
(
)
{
}

export
module V
{
}
}

export A, A1, A2, A3, B, I, C1, D1, D2, D3, E1, E2, E3, X, Y, Z, Wrap, x, y, UU



// Check that 'module' still works as an identifier.

var module
module = {}
module["a"] = 6
function module() {}
function f(module) { return module }
try {} catch (module) {}

module
v = 20
