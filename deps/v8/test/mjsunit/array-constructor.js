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


var loop_count = 5


for (var i = 0; i < loop_count; i++) {
  var a = new Array();
  var b = Array();
  assertEquals(0, a.length);
  assertEquals(0, b.length);
  for (var k = 0; k < 10; k++) {
    assertEquals('undefined', typeof a[k]);
    assertEquals('undefined', typeof b[k]);
  }
}


for (var i = 0; i < loop_count; i++) {
  for (var j = 0; j < 100; j++) {
    var a = new Array(j);
    var b = Array(j);
    assertEquals(j, a.length);
    assertEquals(j, b.length);
    for (var k = 0; k < j; k++) {
      assertEquals('undefined', typeof a[k]);
      assertEquals('undefined', typeof b[k]);
    }
  }
}


for (var i = 0; i < loop_count; i++) {
  a = new Array(0, 1);
  assertArrayEquals([0, 1], a);
  a = new Array(0, 1, 2);
  assertArrayEquals([0, 1, 2], a);
  a = new Array(0, 1, 2, 3);
  assertArrayEquals([0, 1, 2, 3], a);
  a = new Array(0, 1, 2, 3, 4);
  assertArrayEquals([0, 1, 2, 3, 4], a);
  a = new Array(0, 1, 2, 3, 4, 5);
  assertArrayEquals([0, 1, 2, 3, 4, 5], a);
  a = new Array(0, 1, 2, 3, 4, 5, 6);
  assertArrayEquals([0, 1, 2, 3, 4, 5, 6], a);
  a = new Array(0, 1, 2, 3, 4, 5, 6, 7);
  assertArrayEquals([0, 1, 2, 3, 4, 5, 6, 7], a);
  a = new Array(0, 1, 2, 3, 4, 5, 6, 7, 8);
  assertArrayEquals([0, 1, 2, 3, 4, 5, 6, 7, 8], a);
  a = new Array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  assertArrayEquals([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], a);
}


function innerArrayLiteral(n) {
  var a = new Array(n);
  for (var i = 0; i < n; i++) {
    a[i] = i * 2 + 7;
  }
  return a.join();
}


function testConstructOfSizeSize(n) {
  var str = innerArrayLiteral(n);
  var a = eval('[' + str + ']');
  var b = eval('new Array(' + str + ')')
  var c = eval('Array(' + str + ')')
  assertEquals(n, a.length);
  assertArrayEquals(a, b);
  assertArrayEquals(a, c);
}


for (var i = 0; i < loop_count; i++) {
  // JSObject::kInitialMaxFastElementArray is 10000.
  for (var j = 1000; j < 12000; j += 1000) {
    testConstructOfSizeSize(j);
  }
}


for (var i = 0; i < loop_count; i++) {
  assertArrayEquals(['xxx'], new Array('xxx'));
  assertArrayEquals(['xxx'], Array('xxx'));
  assertArrayEquals([true], new Array(true));
  assertArrayEquals([false], Array(false));
  assertArrayEquals([{a:1}], new Array({a:1}));
  assertArrayEquals([{b:2}], Array({b:2}));
}


assertThrows('new Array(3.14)');
assertThrows('Array(2.72)');
