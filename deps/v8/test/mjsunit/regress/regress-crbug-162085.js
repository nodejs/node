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

// Ensure extending an empty packed smi array with a double initializes the
// array with holes.
var a = [1,2,3];
a.length = 0;
a[0] = 1.4;
assertEquals(1.4, a[0]);
assertEquals(undefined, a[1]);
assertEquals(undefined, a[2]);
assertEquals(undefined, a[3]);

// Ensure the double array growstub initializes the array with holes.
function grow_store(a,i,v) {
  a[i] = v;
}

var a2 = [1.3];
grow_store(a2,1,1.4);
a2.length = 0;
grow_store(a2,0,1.5);
assertEquals(1.5, a2[0]);
assertEquals(undefined, a2[1]);
assertEquals(undefined, a2[2]);
assertEquals(undefined, a2[3]);

// Check storing objects using the double grow stub.
var a3 = [1.3];
var o = {};
grow_store(a3, 1, o);
assertEquals(1.3, a3[0]);
assertEquals(o, a3[1]);

// Ensure the double array growstub initializes the array with holes.
function grow_store2(a,i,v) {
  a[i] = v;
}

var a4 = [1.3];
grow_store2(a4,1,1.4);
a4.length = 0;
grow_store2(a4,0,1);
assertEquals(1, a4[0]);
assertEquals(undefined, a4[1]);
assertEquals(undefined, a4[2]);
assertEquals(undefined, a4[3]);
