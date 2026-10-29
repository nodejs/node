// Copyright 2013 the V8 project authors. All rights reserved.
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
    var x = "a" + "b";
    return a; // x, "a", and "b" are dead code
}

function dead2(a, b) {
    var x = a + "0";
    var y = b + "0";
    return a; // x and y are both dead
}

function dead3(a, b) {
    a = a ? "1" : "0";
    b = b ? "1" : "0";
    var x = a + "0";
    var y = b + "0";
    return a; // x and y are both dead
}

assertEquals(33, dead1(33, 32));
assertEquals(33, dead2(33, 32));
assertEquals("1", dead3(33, 32));

assertEquals(31, dead1(31, 30));
assertEquals(31, dead2(31, 30));
assertEquals("1", dead3(31, 32));

assertEquals(0, dead1(0, 30));
assertEquals(0, dead2(0, 30));
assertEquals("0", dead3(0, 32));

assertEquals(true, dead1(true, 0));
assertEquals(true, dead2(true, 0));
assertEquals("1", dead3(true, 0));

assertEquals("true", dead1("true", 0));
assertEquals("true", dead2("true", 0));
assertEquals("1", dead3("true", 0));
