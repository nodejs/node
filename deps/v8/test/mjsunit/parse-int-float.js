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

assertEquals(0, parseInt('0'));
assertEquals(0, parseInt(' 0'));
assertEquals(0, parseInt(' 0 '));

assertEquals(63, parseInt('077'));
assertEquals(63, parseInt('  077'));
assertEquals(63, parseInt('  077   '));
assertEquals(-63, parseInt('  -077'));

assertEquals(3, parseInt('11', 2));
assertEquals(4, parseInt('11', 3));
assertEquals(4, parseInt('11', 3.8));

assertEquals(0x12, parseInt('0x12'));
assertEquals(0x12, parseInt('0x12', 16));
assertEquals(0x12, parseInt('0x12', 16.1));
assertEquals(0x12, parseInt('0x12', NaN));
assertTrue(isNaN(parseInt('0x  ')));
assertTrue(isNaN(parseInt('0x')));
assertTrue(isNaN(parseInt('0x  ', 16)));
assertTrue(isNaN(parseInt('0x', 16)));
assertEquals(12, parseInt('12aaa'));

assertEquals(0.1, parseFloat('0.1'));
assertEquals(0.1, parseFloat('0.1aaa'));
assertEquals(0, parseFloat('0aaa'));
assertEquals(0, parseFloat('0x12'));
assertEquals(77, parseFloat('077'));

assertEquals(Infinity, parseInt('1000000000000000000000000000000000000000000000'
    + '000000000000000000000000000000000000000000000000000000000000000000000000'
    + '000000000000000000000000000000000000000000000000000000000000000000000000'
    + '000000000000000000000000000000000000000000000000000000000000000000000000'
    + '000000000000000000000000000000000000000000000000000000000000000000000000'
    + '0000000000000'));

assertEquals(Infinity, parseInt('0x10000000000000000000000000000000000000000000'
    + '000000000000000000000000000000000000000000000000000000000000000000000000'
    + '000000000000000000000000000000000000000000000000000000000000000000000000'
    + '000000000000000000000000000000000000000000000000000000000000000000000000'
    + '000000000000000000000000000000000000000000000000000000000000000000000000'
    + '0000000000000'));


var i;
var y = 10;

for (i = 1; i < 21; i++) {
  var x = eval("1.2e" + i);
  assertEquals(Math.floor(x), parseInt(x));
  x = eval("1e" + i);
  assertEquals(x, y);
  y *= 10;
  assertEquals(Math.floor(x), parseInt(x));
  x = eval("-1e" + i);
  assertEquals(Math.ceil(x), parseInt(x));
  x = eval("-1.2e" + i);
  assertEquals(Math.ceil(x), parseInt(x));
}

for (i = 21; i < 53; i++) {
  var x = eval("1e" + i);
  assertEquals(1, parseInt(x));
  x = eval("-1e" + i);
  assertEquals(-1, parseInt(x));
}

assertTrue(isNaN(parseInt(0/0)));
assertTrue(isNaN(parseInt(1/0)), "parseInt Infinity");
assertTrue(isNaN(parseInt(-1/0)), "parseInt -Infinity");

assertTrue(isNaN(parseFloat(0/0)));
assertEquals(Infinity, parseFloat(1/0), "parseFloat Infinity");
assertEquals(-Infinity, parseFloat(-1/0), "parseFloat -Infinity");


