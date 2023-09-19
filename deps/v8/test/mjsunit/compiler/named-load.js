// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Foo(a, b) {
  this.a = a;
  this.b = b;
  var bname = "b";
  this.x = this["a"] + this[bname];
}

var f1 = new Foo(3, 4);
assertEquals(7, f1.x);

// SMIs
for (var i = 0; i < 6; i++) {
  var f = new Foo(i, i + 2);
  assertEquals(i + i + 2, f.x);
}

// derbles
for (var i = 0.25; i < 6.25; i++) {
  var f = new Foo(i, i + 2);
  assertEquals(i + i + 2, f.x);
}

// stirngs
for (var i = 0; i < 6; i++) {
  var f = new Foo(i + "", (i + 2) + "");
  assertEquals((i + "") + ((i + 2) + ""), f.x);
}
