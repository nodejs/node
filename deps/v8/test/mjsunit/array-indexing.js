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

var array = [1,2,3,1,2,3,1,2,3,1,2,3];

// ----------------------------------------------------------------------
// Array.prototype.indexOf.
// ----------------------------------------------------------------------

// Negative cases.
assertEquals([].indexOf(1), -1);
assertEquals(array.indexOf(4), -1);
assertEquals(array.indexOf(3, array.length), -1);

assertEquals(array.indexOf(3), 2);
// Negative index out of range.
assertEquals(array.indexOf(1, -17), 0);
// Negative index in rage.
assertEquals(array.indexOf(1, -11), 3);
// Index in range.
assertEquals(array.indexOf(1, 1), 3);
assertEquals(array.indexOf(1, 3), 3);
assertEquals(array.indexOf(1, 4), 6);

// ----------------------------------------------------------------------
// Array.prototype.lastIndexOf.
// ----------------------------------------------------------------------

// Negative cases.
assertEquals([].lastIndexOf(1), -1);
assertEquals(array.lastIndexOf(1, -17), -1);

assertEquals(array.lastIndexOf(1), 9);
// Index out of range.
assertEquals(array.lastIndexOf(1, array.length), 9);
// Index in range.
assertEquals(array.lastIndexOf(1, 2), 0);
assertEquals(array.lastIndexOf(1, 4), 3);
assertEquals(array.lastIndexOf(1, 3), 3);
// Negative index in range.
assertEquals(array.lastIndexOf(1, -11), 0);

