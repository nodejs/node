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

// Flags: --random-seed=17 --allow-natives-syntax
// Flags: --expose-externalize-string

assertEquals("ΚΟΣΜΟΣ ΚΟΣΜΟΣ".toLowerCase(), "κοσμος κοσμος");

var A_CODE = "A".charCodeAt(0);
var Z_CODE = "Z".charCodeAt(0);
var a_CODE = "a".charCodeAt(0);
var z_CODE = "z".charCodeAt(0);

function charCodeToLower(charCode) {
  if (A_CODE <= charCode && charCode <= Z_CODE) {
    return charCode + a_CODE - A_CODE;
  }
  return charCode;
}

function charCodeToUpper(charCode) {
  if (a_CODE <= charCode && charCode <= z_CODE) {
    return charCode - (a_CODE - A_CODE);
  }
  return charCode;
}

function test(length) {
  var str = "";
  var strLower = "";
  var strUpper = "";
  for (var i = 0; i < length; i++) {
    var c = Math.round(0x7f * Math.random());
    str += String.fromCharCode(c);
    strLower += String.fromCharCode(charCodeToLower(c));
    strUpper += String.fromCharCode(charCodeToUpper(c));
  }
  %FlattenString(strLower);
  %FlattenString(strUpper);
  // Sequential string.
  assertEquals(strLower, str.toLowerCase());
  assertEquals(strUpper, str.toUpperCase());
  // Cons string.
  assertEquals(strLower + strLower, (str + str).toLowerCase());
  assertEquals(strUpper + strUpper, (str + str).toUpperCase());
  // Sliced string.
  assertEquals(strLower.substring(1), str.substring(1).toLowerCase());
  assertEquals(strUpper.substring(1), str.substring(1).toUpperCase());
  // External string.
  externalizeString(str, false);
  assertEquals(strLower, str.toLowerCase());
  assertEquals(strUpper, str.toUpperCase());
}

for (var i = 1; i <= 128; i <<= 1); {
  for (var j = 0; j < 8; j++) {
    for (var k = 0; k < 3; k++) {
      test(i + j);
    }
  }
}
