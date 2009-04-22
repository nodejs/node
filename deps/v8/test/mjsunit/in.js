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

// ----------------
// Check fast objects

var o = { };
assertFalse(0 in o);
assertFalse('x' in o);
assertFalse('y' in o);
assertTrue('toString' in o, "toString");

var o = { x: 12 };
assertFalse(0 in o);
assertTrue('x' in o);
assertFalse('y' in o);
assertTrue('toString' in o, "toString");

var o = { x: 12, y: 15 };
assertFalse(0 in o);
assertTrue('x' in o);
assertTrue('y' in o);
assertTrue('toString' in o, "toString");


// ----------------
// Check dense arrays

var a = [ ];
assertFalse(0 in a);
assertFalse(1 in a);
assertFalse('0' in a);
assertFalse('1' in a);
assertTrue('toString' in a, "toString");

var a = [ 1 ];
assertTrue(0 in a);
assertFalse(1 in a);
assertTrue('0' in a);
assertFalse('1' in a);
assertTrue('toString' in a, "toString");

var a = [ 1, 2 ];
assertTrue(0 in a);
assertTrue(1 in a);
assertTrue('0' in a);
assertTrue('1' in a);
assertTrue('toString' in a, "toString");

var a = [ 1, 2 ];
assertFalse(0.001 in a);
assertTrue(-0 in a);
assertTrue(+0 in a);
assertFalse('0.0' in a);
assertFalse('1.0' in a);
assertFalse(NaN in a);
assertFalse(Infinity in a);
assertFalse(-Infinity in a);

var a = [];
a[1] = 2;
assertFalse(0 in a);
assertTrue(1 in a);
assertFalse(2 in a);
assertFalse('0' in a); 
assertTrue('1' in a);
assertFalse('2' in a);
assertTrue('toString' in a, "toString");


// ----------------
// Check dictionary ("normalized") objects

var o = {};
for (var i = 0x0020; i < 0x02ff; i += 2) {
  o['char:' + String.fromCharCode(i)] = i;
}
for (var i = 0x0020; i < 0x02ff; i += 2) {
  assertTrue('char:' + String.fromCharCode(i) in o);
  assertFalse('char:' + String.fromCharCode(i + 1) in o);
}
assertTrue('toString' in o, "toString");

var o = {};
o[Math.pow(2,30)-1] = 0;
o[Math.pow(2,31)-1] = 0;
o[1] = 0;
assertFalse(0 in o);
assertTrue(1 in o);
assertFalse(2 in o);
assertFalse(Math.pow(2,30)-2 in o);
assertTrue(Math.pow(2,30)-1 in o);
assertFalse(Math.pow(2,30)-0 in o);
assertTrue(Math.pow(2,31)-1 in o);
assertFalse(0.001 in o);
assertFalse('0.0' in o);
assertFalse('1.0' in o);
assertFalse(NaN in o);
assertFalse(Infinity in o);
assertFalse(-Infinity in o);
assertFalse(-0 in o);
assertFalse(+0 in o);
assertTrue('toString' in o, "toString");


// ----------------
// Check sparse arrays

var a = [];
a[Math.pow(2,30)-1] = 0;
a[Math.pow(2,31)-1] = 0;
a[1] = 0;
assertFalse(0 in a, "0 in a");
assertTrue(1 in a, "1 in a");
assertFalse(2 in a, "2 in a");
assertFalse(Math.pow(2,30)-2 in a, "Math.pow(2,30)-2 in a");
assertTrue(Math.pow(2,30)-1 in a, "Math.pow(2,30)-1 in a");
assertFalse(Math.pow(2,30)-0 in a, "Math.pow(2,30)-0 in a");
assertTrue(Math.pow(2,31)-1 in a, "Math.pow(2,31)-1 in a");
assertFalse(0.001 in a, "0.001 in a");
assertFalse('0.0' in a,"'0.0' in a");
assertFalse('1.0' in a,"'1.0' in a");
assertFalse(NaN in a,"NaN in a");
assertFalse(Infinity in a,"Infinity in a");
assertFalse(-Infinity in a,"-Infinity in a");
assertFalse(-0 in a,"-0 in a");
assertFalse(+0 in a,"+0 in a");
assertTrue('toString' in a, "toString");

// -------------
// Check negative indices in arrays.
var a = [];
assertFalse(-1 in a);
a[-1] = 43;
assertTrue(-1 in a);
