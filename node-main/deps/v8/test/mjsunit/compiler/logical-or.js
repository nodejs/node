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

function OrBB(x,y) {
  return (x == 0) || (y == 0);
}

function OrBN(x,y) {
  return (x == 0) || y;
}

function OrNB(x,y) {
  return x || (y == 0);
}

function OrNN(x,y) {
  return x || y;
}

assertTrue(OrBB(0, 0));
assertTrue(OrBB(1, 0));
assertTrue(OrBB(0, 1));
assertFalse(OrBB(1, 1));

assertTrue(OrBN(0, 0));
assertEquals(0, OrBN(1, 0));
assertTrue(OrBN(0, 1));
assertEquals(1, OrBN(1, 1));
assertEquals(2, OrBN(1, 2));

assertTrue(OrNB(0, 0));
assertEquals(1, OrNB(1, 0));
assertFalse(OrNB(0, 1));
assertEquals(1, OrNB(1, 1));
assertEquals(2, OrNB(2, 1));

assertEquals(0, OrNN(0, 0));
assertEquals(1, OrNN(1, 0));
assertEquals(2, OrNN(2, 0));
assertEquals(1, OrNN(0, 1));
assertEquals(2, OrNN(0, 2));
assertEquals(1, OrNN(1, 2));
