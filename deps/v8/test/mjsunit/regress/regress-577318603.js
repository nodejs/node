// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-js-nonextensible-applies-to-private

class Base {
  constructor(o) {
    return o;
  }
}

class InstallPrivateField extends Base {
  #p = 1;
  static setP(o, v) {
    o.#p = v;
  }
  static getP(o) {
    return o.#p;
  }
}

// Private field added onto a non-extensible root map before Object.seal.
{
  const obj = new Date();
  obj[0] = 1;
  obj.x = 1;

  Object.preventExtensions(obj);
  Object.defineProperty(obj, 0, { enumerable: false });

  new InstallPrivateField(obj);
  Object.seal(obj);
  InstallPrivateField.setP(obj, 1.5);

  assertEquals(1.5, InstallPrivateField.getP(obj));
  assertFalse(Object.getOwnPropertyDescriptor(obj, 'x').configurable);
}

// Object.seal on a non-extensible root map before adding a private field.
{
  const obj = new Date();
  obj[0] = 1;
  obj.x = 1;

  Object.preventExtensions(obj);
  Object.defineProperty(obj, 0, { enumerable: false });

  Object.seal(obj);
  new InstallPrivateField(obj);
  InstallPrivateField.setP(obj, 1.5);

  assertEquals(1.5, InstallPrivateField.getP(obj));
  assertFalse(Object.getOwnPropertyDescriptor(obj, 'x').configurable);
}

// Constness generalization of a root-map property after Object.seal on a
// non-extensible dictionary-elements root map must not drop the seal
// transition.
{
  const obj = new Date();
  obj[0] = 1;
  obj.x = 1;

  Object.preventExtensions(obj);
  Object.defineProperty(obj, 0, { enumerable: false });

  Object.seal(obj);
  obj.x = 2;

  assertEquals(2, obj.x);
  assertTrue(Object.isSealed(obj));
  assertFalse(Object.getOwnPropertyDescriptor(obj, 'x').configurable);
}
