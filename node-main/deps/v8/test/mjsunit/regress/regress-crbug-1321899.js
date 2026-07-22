// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class A {
  constructor(arg) {
    return arg;
  }
}

class B extends A {
  #b = 1;  // ACCESS_CHECK -> DATA
  constructor(arg) {
    super(arg);
  }
  static setField(obj) {
    obj.#b = 'b';  // KeyedStoreIC
  }
  static getField(obj) {
    return obj.#b;
  }
  static hasField(obj) {
    return #b in obj;
  }
}

class C extends A {
  #c;  // DefineKeyedOwnIC: ACCESS_CHECK -> NOT_FOUND
  constructor(arg) {
    super(arg);
  }
  static setField(obj) {
    obj.#c = 'c';  // KeyedStoreIC
  }
  static getField(obj) {
    return obj.#c;
  }
  static hasField(obj) {
    return #c in obj;
  }
}

let d = 0;
class D extends A {
  get #d() { return d; }
  set #d(val) { d = val; }
  constructor(arg) {
    super(arg);  // KeyedStoreIC for private brand
  }
  static setAccessor(obj) {
    obj.#d = 'd';  // KeyedLoadIC for private brand
  }
  static getAccessor(obj) {
    return obj.#d;  // KeyedLoadIC for private brand
  }
  static hasAccessor(obj) {
    return #d in obj;
  }
}

class E extends A {
  #e() { return 0; }
  constructor(arg) {
    super(arg);  // KeyedStoreIC for private brand
  }
  static setMethod(obj) {
    obj.#e = 'e';  // KeyedLoadIC for private brand
  }
  static getMethod(obj) {
    return obj.#e;  // KeyedLoadIC for private brand
  }
  static hasMethod(obj) {
    return #e in obj;
  }
}

function checkHasAccess(object) {
  assertThrows(() => B.setField(globalProxy), TypeError, /Cannot write private member #b to an object whose class did not declare it/);
  assertThrows(() => B.getField(globalProxy), TypeError, /Cannot read private member #b from an object whose class did not declare it/);
  assertFalse(B.hasField(globalProxy));

  new B(globalProxy);
  assertEquals(B.getField(globalProxy), 1);
  B.setField(globalProxy);
  assertEquals(B.getField(globalProxy), 'b');  // Fast case
  B.setField(globalProxy);  // Fast case
  assertEquals(B.getField(globalProxy), 'b');  // Fast case
  assertThrows(() => new B(globalProxy), TypeError, /Cannot initialize #b twice on the same object/);
  assertTrue(B.hasField(globalProxy));
  assertTrue(B.hasField(globalProxy));  // Fast case

  assertThrows(() => C.setField(globalProxy), TypeError, /Cannot write private member #c to an object whose class did not declare it/);
  assertThrows(() => C.getField(globalProxy), TypeError, /Cannot read private member #c from an object whose class did not declare it/);
  assertFalse(C.hasField(globalProxy));

  new C(globalProxy);
  assertEquals(C.getField(globalProxy), undefined);
  C.setField(globalProxy);
  assertEquals(C.getField(globalProxy), 'c');  // Fast case
  C.setField(globalProxy);  // Fast case
  assertEquals(C.getField(globalProxy), 'c');  // Fast case
  assertThrows(() => new C(globalProxy), TypeError, /Cannot initialize #c twice on the same object/);
  assertTrue(C.hasField(globalProxy));
  assertTrue(C.hasField(globalProxy));  // Fast case

  assertThrows(() => D.setAccessor(globalProxy), TypeError, /Receiver must be an instance of class D/);
  assertThrows(() => D.getAccessor(globalProxy), TypeError, /Receiver must be an instance of class D/);
  assertFalse(D.hasAccessor(globalProxy));

  new D(globalProxy);
  assertEquals(D.getAccessor(globalProxy), 0);
  D.setAccessor(globalProxy);
  assertEquals(D.getAccessor(globalProxy), 'd');  // Fast case
  D.setAccessor(globalProxy);  // Fast case
  assertEquals(D.getAccessor(globalProxy), 'd');  // Fast case
  assertThrows(() => new D(globalProxy), TypeError, /Cannot initialize private methods of class D twice on the same object/);
  assertTrue(D.hasAccessor(globalProxy));
  assertTrue(D.hasAccessor(globalProxy));  // Fast case

  assertThrows(() => E.setMethod(globalProxy), TypeError, /Receiver must be an instance of class E/);
  assertThrows(() => E.getMethod(globalProxy), TypeError, /Receiver must be an instance of class E/);
  assertFalse(E.hasMethod(globalProxy));

  new E(globalProxy);
  assertEquals(E.getMethod(globalProxy)(), 0);
  assertThrows(() => E.setMethod(globalProxy), TypeError, /Private method '#e' is not writable/);
  assertEquals(E.getMethod(globalProxy)(), 0);  // Fast case
  assertThrows(() => new E(globalProxy), TypeError, /Cannot initialize private methods of class E twice on the same object/);
  assertTrue(E.hasMethod(globalProxy));
  assertTrue(E.hasMethod(globalProxy));  // Fast case
}

function checkNoAccess(object, message) {
  assertThrows(() => new B(object), Error, message);
  assertThrows(() => new C(object), Error, message);
  assertThrows(() => new D(object), Error, message);
  assertThrows(() => new E(object), Error, message);
  assertThrows(() => B.setField(object), Error, message);
  assertThrows(() => B.getField(object), Error, message);
  assertThrows(() => B.hasField(object), Error, message);
  assertThrows(() => C.setField(object), Error, message);
  assertThrows(() => C.getField(object), Error, message);
  assertThrows(() => C.hasField(object), Error, message);
  assertThrows(() => D.setAccessor(object), Error, message);
  assertThrows(() => D.getAccessor(object), Error, message);
  assertThrows(() => D.hasAccessor(object), Error, message);
  assertThrows(() => E.setMethod(object), Error, message);
  assertThrows(() => E.getMethod(object), Error, message);
  assertThrows(() => E.hasMethod(object), Error, message);
}
