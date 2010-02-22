// Copyright 2009 the V8 project authors. All rights reserved.
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

// Flags: --nofull-compiler --nofast-compiler

// Test paths in the code generator where values in specific registers
// get moved around.
function identity(x) {
  return x;
}

function cover_codegen_paths() {
  var x = 1;

  // This test depends on the fixed order of register allocation.  We try to
  // get values in specific registers (ia32, x64):
  var a;   // Register eax, rax.
  var b;   // Register ebx, rbx.
  var c;   // Register ecx, rcx.
  var d;   // Register edx, rdx.
  var di;  // Register edi, rdi.

  while (x == 1) {
    // The call will spill registers and leave x in {eax,rax}.
    x = identity(1);
    // The add will spill x and reuse {eax,rax} for the result.
    a = x + 1;
    // A fresh register {ebx,rbx} will be allocated for x, then reused for
    // the result.
    b = x + 1;
    // Et cetera.
    c = x + 1;
    d = x + 1;
    di = x + 1;
    // Locals are in the corresponding registers here.
    assertEquals(8, c << a);

    x = identity(1);
    a = x + 1;
    b = x + 1;
    c = x + 1;
    d = x + 1;
    di = x + 1;
    assertEquals(8, a << c);

    x = identity(1);
    a = x + 1;
    b = x + 1;
    c = x + 1;
    d = x + 1;
    di = x + 1;
    c = 0; // Free register ecx.
    assertEquals(8, a << d);

    x = identity(1);
    a = x + 1;
    b = x + 1;
    c = x + 1;
    d = x + 1;
    di = x + 1;
    b = 0; // Free register ebx.
    assertEquals(8, a << d);

    // Test the non-commutative subtraction operation with a smi on the
    // left, all available registers on the right, and a non-smi result.
    x = identity(-1073741824);  // Least (31-bit) smi.
    a = x + 1;  // Still a smi, the greatest smi negated.
    b = x + 1;
    c = x + 1;
    d = x + 1;
    di = x + 1;
    // Subtraction should overflow the 31-bit smi range.  The result
    // (1073741824) is outside the 31-bit smi range so it doesn't hit the
    // "unsafe smi" code that spills a register.
    assertEquals(1073741824, 1 - a);

    x = identity(-1073741824);
    a = x + 1;
    b = x + 1;
    c = x + 1;
    d = x + 1;
    di = x + 1;
    assertEquals(1073741824, 1 - b);

    x = identity(-1073741824);
    a = x + 1;
    b = x + 1;
    c = x + 1;
    d = x + 1;
    di = x + 1;
    assertEquals(1073741824, 1 - c);

    x = identity(-1073741824);
    a = x + 1;
    b = x + 1;
    c = x + 1;
    d = x + 1;
    di = x + 1;
    assertEquals(1073741824, 1 - d);

    x = identity(-1073741824);
    a = x + 1;
    b = x + 1;
    c = x + 1;
    d = x + 1;
    di = x + 1;
    assertEquals(1073741824, 1 - di);

    x = 3;
  }
}

cover_codegen_paths();
