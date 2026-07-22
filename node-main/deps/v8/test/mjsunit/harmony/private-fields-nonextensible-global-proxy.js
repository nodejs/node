// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Base {
  constructor(arg) {
    return arg;
  }
}

class ClassNonExtensibleWithPrivateField extends Base {
  #privateField = (() => {
    Object.preventExtensions(this);
    return "defined";
  })();
  // In case the object has a null prototype, we'll use a static
  // method to access the field.
  static getPrivateField(obj) { return obj.#privateField; }
  constructor(arg) {
    super(arg);
  }
}

new ClassNonExtensibleWithPrivateField(globalThis);
assertEquals("defined", ClassNonExtensibleWithPrivateField.getPrivateField(globalThis));
