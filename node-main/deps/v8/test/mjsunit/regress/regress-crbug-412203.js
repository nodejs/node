// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var b = [];
b[10000] = 1;
// Required to reproduce the bug.
assertTrue(%HasDictionaryElements(b));

var a1 = [1.5];
b.__proto__ = a1;
assertEquals(1.5, ([].concat(b))[0]);

var a2 = new Int32Array(2);
a2[0] = 3;
b.__proto__ = a2
assertEquals(3, ([].concat(b))[0]);

function foo(x, y) {
  var a = [];
  a[10000] = 1;
  assertTrue(%HasDictionaryElements(a));

  a.__proto__ = arguments;
  var c = [].concat(a);
  for (var i = 0; i < arguments.length; i++) {
    assertEquals(i + 2, c[i]);
  }
  assertEquals(undefined, c[arguments.length]);
  assertEquals(undefined, c[arguments.length + 1]);
}
foo(2);
foo(2, 3);
foo(2, 3, 4);
