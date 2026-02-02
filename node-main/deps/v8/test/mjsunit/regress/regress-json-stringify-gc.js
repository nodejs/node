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

var a = [];
var new_space_string = "a";
for (var i = 0; i < 8; i++) {
  new_space_string += new_space_string;
}
for (var i = 0; i < 10000; i++) a.push(new_space_string);

// At some point during the first stringify, allocation causes a GC and
// new_space_string is moved to old space. Make sure that this does not
// screw up reading from the correct location.
json1 = JSON.stringify(a);
json2 = JSON.stringify(a);
assertTrue(json1 == json2, "GC caused JSON.stringify to fail.");

// Check that the slow path of JSON.stringify works correctly wrt GC.
for (var i = 0; i < 10000; i++) {
  var s = i.toString();
  assertEquals('"' + s + '"', JSON.stringify(s, null, 0));
}

for (var i = 0; i < 10000; i++) {
  var s = i.toString() + "\u2603";
  assertEquals('"' + s + '"', JSON.stringify(s, null, 0));
}
