// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


class Base {}

class DerivedWithReturn extends Base {
  constructor(x) {
    super();
    return x;
  }
}

assertThrows(function() {
  new DerivedWithReturn(null);
}, TypeError);
assertThrows(function() {
  new DerivedWithReturn(42);
}, TypeError);
assertThrows(function() {
  new DerivedWithReturn(true);
}, TypeError);
assertThrows(function() {
  new DerivedWithReturn('hi');
}, TypeError);
assertThrows(function() {
  new DerivedWithReturn(Symbol());
}, TypeError);


assertInstanceof(new DerivedWithReturn(undefined), DerivedWithReturn);
function f() {}
assertInstanceof(new DerivedWithReturn(new f()), f);
assertInstanceof(new DerivedWithReturn(/re/), RegExp);


class DerivedWithReturnNoSuper extends Base {
  constructor(x) {
    return x;
  }
}


assertThrows(function() {
  new DerivedWithReturnNoSuper(null);
}, TypeError);
assertThrows(function() {
  new DerivedWithReturnNoSuper(42);
}, TypeError);
assertThrows(function() {
  new DerivedWithReturnNoSuper(true);
}, TypeError);
assertThrows(function() {
  new DerivedWithReturnNoSuper('hi');
}, TypeError);
assertThrows(function() {
  new DerivedWithReturnNoSuper(Symbol());
}, TypeError);
assertThrows(function() {
  new DerivedWithReturnNoSuper(undefined);
}, ReferenceError);


function f2() {}
assertInstanceof(new DerivedWithReturnNoSuper(new f2()), f2);
assertInstanceof(new DerivedWithReturnNoSuper(/re/), RegExp);


class DerivedReturn extends Base {
  constructor() {
    super();
    return;
  }
}

assertInstanceof(new DerivedReturn(), DerivedReturn);



class DerivedReturnThis extends Base {
  constructor() {
    super();
    return this;
  }
}

assertInstanceof(new DerivedReturnThis(), DerivedReturnThis);
