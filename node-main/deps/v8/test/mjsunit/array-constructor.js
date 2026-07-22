// Copyright 2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

function testConstruction(len, elements_str) {
  var a = eval('[' + elements_str + ']');
  var b = eval('new Array(' + elements_str + ')')
  var c = eval('Array(' + elements_str + ')')
  assertEquals(len, a.length);
  assertArrayEquals(a, b);
  assertArrayEquals(a, c);
}

for (var i = 0; i < loop_count; i++) {
  const N = 2000;
  const literal = innerArrayLiteral(N);
  let str = literal;
  // JSObject::kInitialMaxFastElementArray is approximately 10000.
  for (var j = N; j <= 12000; j += N) {
    testConstruction(j, str);
    str += ", " + literal;
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

// Make sure that throws occur in the context of the Array function.
var b = Realm.create();
var bArray = Realm.eval(b, "Array");
var bError = Realm.eval(b, "RangeError");

function verifier(array, error) {
  try {
    new array(3.14);
  } catch(e) {
    return e.__proto__ === error.__proto__;
  }
  assertTrue(false);  // should never get here.
}


assertTrue(verifier(Array, RangeError()));
assertTrue(verifier(bArray, bError()));
assertFalse(verifier(Array, bError()));
assertFalse(verifier(bArray, RangeError()));
