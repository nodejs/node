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

function dead1(a, b) {
    a + b; // dead
    return a;
}

function dead2(a, b) {
    a | 0; // dead
    b | 0; // dead
    return a; // x and y are both dead
}

function dead3(a, b) {
    a == 2 ? a : b; // dead
    return a;
}

function dead4(a) {
    var z = 3;
    for (i = 0; i < 3; i++) {
        z + 3; // dead
    }
    return a;
}

function dead5(a) {
    var z = 3;
    for (i = 0; i < 3; i++) {
        z + 3; // dead
        z++;
    }
    var w = z + a;
    return a; // z is dead
}

assertTrue(dead1(33, 32) == 33);
assertTrue(dead2(33, 32) == 33);
assertTrue(dead3(33, 32) == 33);
assertTrue(dead4(33) == 33);
assertTrue(dead5(33) == 33);

assertTrue(dead1(34, 7) == 34);
assertTrue(dead2(34, 7) == 34);
assertTrue(dead3(34, 7) == 34);
assertTrue(dead4(34) == 34);
assertTrue(dead5(34) == 34);

assertTrue(dead1(3.4, 0.1) == 3.4);
assertTrue(dead2(3.4, 0.1) == 3.4);
assertTrue(dead3(3.4, 0.1) == 3.4);
assertTrue(dead4(3.4) == 3.4);
assertTrue(dead5(3.4) == 3.4);
