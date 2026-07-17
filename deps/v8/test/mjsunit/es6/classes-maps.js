// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax
'use strict';

(function TestMaps() {
  class Base {}
  class Derived extends Base {}

  let d1 = new Derived();
  let d2 = new Derived();

  assertTrue(%HaveSameMap(d1, d2));
}());


(function TestProtoModificationArray() {
  let called = 0;
  function F() {
    called++;
    assertFalse(Array.isArray(this));
  }
  class Derived extends Array {}
  assertSame(Derived.__proto__, Array);

  let d1 = new Derived();
  assertTrue(Array.isArray(d1));

  Derived.__proto__ = F;
  called = 0;
  let d2 = new Derived();
  assertSame(1, called);
  assertFalse(Array.isArray(d2));

  assertFalse(%HaveSameMap(d1, d2));
}());


(function TestProtoModification() {
  let called = 0;
  function F() {
    called++;
    let exn = null;
    try {
      this.byteLength;
    } catch (e) {
      exn = e;
    }
    assertTrue(exn instanceof TypeError);
  }
  class Derived extends Uint8Array {
    constructor() { super(10); }
  }
  assertSame(Derived.__proto__, Uint8Array);

  let d1 = new Derived();
  assertSame(10, d1.byteLength);

  Derived.__proto__ = F;
  called = 0;
  let d2 = new Derived();
  assertSame(1, called);
  assertThrows(function() { d2.byteLength; }, TypeError);

  assertFalse(%HaveSameMap(d1, d2));
}());
