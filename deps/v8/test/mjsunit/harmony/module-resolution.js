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

// Test basic module interface inference.

"use strict";

print("begin.")

export let x = print("0")

export module B = A.B

export module A {
  export let x = print("1")
  export let f = function() { return B.x }
  export module B {
    module BB = B
    export BB, x
    let x = print("2")
    let y = print("3")
    let Ax = A.x
    let ABx = A.B.x
    let Ay = A.y
    let BBx = BB.x
    let Af = A.f
    function f(x,y) { return x }
  }
  export let y = print("4")
  let Ax = A.x
  let Bx = B.x
  let ABx = A.B.x
  module C {
    export let z = print("5")
    export module D = B
    // TODO(rossberg): turn these into proper negative test cases once we have
    // suitable error messages.
    // import C.z  // multiple declarations
    import x from B
  }
  module D {
    // TODO(rossberg): Handle import *.
    // import A.*  // invalid forward import
  }
  module M {}
  // TODO(rossberg): Handle import *.
  // import M.*  // invalid forward import
  let Cz = C.z
  let CDx = C.D.x
}

export module Imports {
  module A1 {
    export module A2 {}
  }
  module B {
    // TODO(rossberg): Handle import *.
    // import A1.*
    // import A2.*  // unbound variable A2
  }
}

export module E {
  export let xx = x
  export y, B
  let Bx = B.x
  // TODO(rossberg): Handle import *.
  // import A.*
}

export module M1 {
  export module A2 = M2
}
export module M2 {
  export module A1 = M1
}

// TODO(rossberg): turn these into proper negative test cases once we have
// suitable error messages.
// module W1 = W2.W
// module W2 = { export module W = W3 }
// module W3 = W1  // cyclic module definition

// module W1 = W2.W3
// module W2 = {
//   export module W3 = W4
//   export module W4 = W1
// }  // cyclic module definition

// TODO(rossberg): Handle import *.
//module M3B = M3.B
//export module M3 {
//  export module B { export let x = "" }
//  module C1 = { import M3.* }
//  module C2 = { import M3.B.* }
//  module C3 = { import M3B.* }
//  module C4 = { export x import B.* }
//// TODO(rossberg): turn these into proper negative test cases once we have
//// suitable error messages.
//// export module C5 = { import C5.* }  // invalid forward import
//// export module C6 = { import M3.C6.* }  // invalid forward import
//}

export module External at "external.js"
export module External1 = External
export module ExternalA = External.A
export module InnerExternal {
  export module E at "external.js"
}
export module External2 = InnerExternal.E
//export let xxx = InnerExternal.E.A.x

print("end.")
