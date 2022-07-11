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
}

let d = 0;
class D extends A {
  get #d() { return d; }
  set #d(val) { d = val;}
  constructor(arg) {
    super(arg);  // KeyedStoreIC for private brand
  }
  static setAccessor(obj) {
    obj.#d = 'd';  // KeyedLoadIC for private brand
  }
  static getAccessor(obj) {
    return obj.#d;  // KeyedLoadIC for private brand
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
}
