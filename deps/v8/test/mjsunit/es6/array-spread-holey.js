// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test spreading of holey arrays. Holes should be replaced with undefined.

var a = [, 2];

assertEquals([, 2], [...a]);
assertTrue([...a].hasOwnProperty(0));
assertTrue([2, ...a].hasOwnProperty(1));


class MyArray1 extends Array {
  constructor(a) {
    super(...a);
  }
}
var myarr1 = new MyArray1(a);
assertEquals(undefined, myarr1[0]);
assertTrue(myarr1.hasOwnProperty(0));


class MyArray2 extends Array {
  constructor(a) {
    super(2, ...a);
  }
}
var myarr2 = new MyArray2(a);
assertEquals(undefined, myarr2[1]);
assertTrue(myarr2.hasOwnProperty(1));

function foo0() { return arguments.hasOwnProperty(0); }
assertTrue(foo0(...a));

function foo1() { return arguments.hasOwnProperty(1); }
assertTrue(foo1(2, ...a));

// This test pollutes the Array prototype. No more tests should be run in the
// same instance after this.
a.__proto__[0] = 1;
var arr2 = [...a];
assertEquals([1,2], arr2);
assertTrue(arr2.hasOwnProperty(0));

myarr1 = new MyArray1(a);
assertEquals(1, myarr1[0]);
assertTrue(myarr1.hasOwnProperty(0));

var myarr2 = new MyArray2(a);
assertEquals(1, myarr2[1]);
assertTrue(myarr2.hasOwnProperty(1));
