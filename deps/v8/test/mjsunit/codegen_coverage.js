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

// Test the paths in the code generator where values in specific
// registers get moved around so that the shift operation can use
// register ECX on ia32 for the shift amount.  Other codegen coverage
// tests should go here too.



function identity(x) {
  return x;
}

function cover_codegen_paths() {
  var x = 1;
  var a;  // Register EAX
  var b;  // Register EBX
  var c;  // Register ECX
  var d;  // Register EDX
  // Register ESI is already used.
  var di;  // Register EDI

  while (x == 1) {
    x = identity(1);
    a = x + 1;
    c = x + 1;
    d = x + 1;
    b = x + 1;
    di = x + 1;
    // Locals are in the corresponding registers here.
    assertEquals(c << a, 8);

    x = identity(1);
    a = x + 1;
    c = x + 1;
    d = x + 1;
    b = x + 1;
    di = x + 1;
    // Locals are in the corresponding registers here.
    assertEquals(a << c, 8);

    x = identity(1);
    a = x + 1;
    c = x + 1;
    d = x + 1;
    b = x + 1;
    di = x + 1;
    // Locals are in the corresponding registers here.
    c = 0; // Free register ecx.
    assertEquals(a << d, 8);

    x = identity(1);
    a = x + 1;
    c = x + 1;
    d = x + 1;
    b = x + 1;
    di = x + 1;
    // Locals are in the corresponding registers here.
    b = 0; // Free register ebx.
    assertEquals(a << d, 8);

    x = 3;
  }
}

cover_codegen_paths();
