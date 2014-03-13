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

// Flags: --new-string-add

assertEquals("ab", "a" + "b", "ll");

assertEquals("12", "1" + "2", "dd");
assertEquals("123", "1" + "2" + "3", "ddd");
assertEquals("123", 1 + "2" + "3", "ndd");
assertEquals("123", "1" + 2 + "3", "dnd");
assertEquals("123", "1" + "2" + 3, "ddn");

assertEquals("123", "1" + 2 + 3, "dnn");
assertEquals("123", 1 + "2" + 3, "ndn");
assertEquals("33", 1 + 2 + "3", "nnd");

var x = "1";
assertEquals("12", x + 2, "vn");
assertEquals("12", x + "2", "vd");
assertEquals("21", 2 + x, "nv");
assertEquals("21", "2" + x, "dv");

var y = "2";
assertEquals("12", x + y, "vdvd");

x = 1;
assertEquals("12", x + y, "vnvd");

y = 2;
assertEquals(3, x + y, "vnvn");

x = "1";
assertEquals("12", x + y, "vdvn");

y = "2";
assertEquals("12", x + y, "vdvd2");

(function(x, y) {
  var z = "3";
  var w = "4";

  assertEquals("11", x + x, "xx");
  assertEquals("12", x + y, "xy");
  assertEquals("13", x + z, "xz");
  assertEquals("14", x + w, "xw");

  assertEquals("21", y + x, "yx");
  assertEquals("22", y + y, "yy");
  assertEquals("23", y + z, "yz");
  assertEquals("24", y + w, "yw");

  assertEquals("31", z + x, "zx");
  assertEquals("32", z + y, "zy");
  assertEquals("33", z + z, "zz");
  assertEquals("34", z + w, "zw");

  assertEquals("41", w + x, "wx");
  assertEquals("42", w + y, "wy");
  assertEquals("43", w + z, "wz");
  assertEquals("44", w + w, "ww");

  (function(){x = 1; z = 3;})();

  assertEquals(2, x + x, "x'x");
  assertEquals("12", x + y, "x'y");
  assertEquals(4, x + z, "x'z'");
  assertEquals("14", x + w, "x'w");

  assertEquals("21", y + x, "yx'");
  assertEquals("22", y + y, "yy");
  assertEquals("23", y + z, "yz'");
  assertEquals("24", y + w, "yw");

  assertEquals(4, z + x, "z'x'");
  assertEquals("32", z + y, "z'y");
  assertEquals(6, z + z, "z'z'");
  assertEquals("34", z + w, "z'w");

  assertEquals("41", w + x, "wx'");
  assertEquals("42", w + y, "wy");
  assertEquals("43", w + z, "wz'");
  assertEquals("44", w + w, "ww");
})("1", "2");

assertEquals("142", "1" + new Number(42), "sN");
assertEquals("421", new Number(42) + "1", "Ns");
assertEquals(84, new Number(42) + new Number(42), "NN");

assertEquals("142", "1" + new String("42"), "sS");
assertEquals("421", new String("42") + "1", "Ss");
assertEquals("142", "1" + new String("42"), "sS");
assertEquals("4242", new String("42") + new String("42"), "SS");

assertEquals("1true", "1" + true, "sb");
assertEquals("true1", true + "1", "bs");
assertEquals(2, true + true, "bs");

assertEquals("1true", "1" + new Boolean(true), "sB");
assertEquals("true1", new Boolean(true) + "1", "Bs");
assertEquals(2, new Boolean(true) + new Boolean(true), "Bs");

assertEquals("1undefined", "1" + void 0, "sv");
assertEquals("undefined1", (void 0)  + "1", "vs");
assertTrue(isNaN(void 0 + void 0), "vv");

assertEquals("1null", "1" + null, "su");
assertEquals("null1", null + "1", "us");
assertEquals(0, null + null, "uu");

(function (i) {
  // Check that incoming frames are merged correctly.
  var x;
  var y;
  var z;
  var w;
  switch (i) {
  case 1: x = 42; y = "stry"; z = "strz"; w = 42; break;
  default: x = "strx", y = 42; z = "strz"; w = 42; break;
  }
  var resxx = x + x;
  var resxy = x + y;
  var resxz = x + z;
  var resxw = x + w;
  var resyx = y + x;
  var resyy = y + y;
  var resyz = y + z;
  var resyw = y + w;
  var reszx = z + x;
  var reszy = z + y;
  var reszz = z + z;
  var reszw = z + w;
  var reswx = w + x;
  var reswy = w + y;
  var reswz = w + z;
  var resww = w + w;
  assertEquals(84, resxx, "swxx");
  assertEquals("42stry", resxy, "swxy");
  assertEquals("42strz", resxz, "swxz");
  assertEquals(84, resxw, "swxw");
  assertEquals("stry42", resyx, "swyx");
  assertEquals("strystry", resyy, "swyy");
  assertEquals("strystrz", resyz, "swyz");
  assertEquals("stry42", resyw, "swyw");
  assertEquals("strz42", reszx, "swzx");
  assertEquals("strzstry", reszy, "swzy");
  assertEquals("strzstrz", reszz, "swzz");
  assertEquals("strz42", reszw, "swzw");
  assertEquals(84, reswx, "swwx");
  assertEquals("42stry", reswy, "swwy");
  assertEquals("42strz", reswz, "swwz");
  assertEquals(84, resww, "swww");
})(1);

// Generate ascii and non ascii strings from length 0 to 20.
var ascii = 'aaaaaaaaaaaaaaaaaaaa';
var non_ascii = '\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234\u1234';
assertEquals(20, ascii.length);
assertEquals(20, non_ascii.length);
var a = Array(21);
var b = Array(21);
for (var i = 0; i <= 20; i++) {
  a[i] = ascii.substring(0, i);
  b[i] = non_ascii.substring(0, i);
}

// Add ascii and non-ascii strings generating strings with length from 0 to 20.
for (var i = 0; i <= 20; i++) {
  for (var j = 0; j < i; j++) {
    assertEquals(a[i], a[j] + a[i - j])
    assertEquals(b[i], b[j] + b[i - j])
  }
}
