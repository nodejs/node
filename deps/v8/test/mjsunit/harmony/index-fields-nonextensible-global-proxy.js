// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Base {
  constructor(arg) {
    return arg;
  }
}

class ClassNonExtensibleWithIndexField extends Base {
  [0] = (() => {
    Object.preventExtensions(this);
    return 'defined';
  })();
  ['nonExtensible'] = 4;
  constructor(arg) {
    super(arg);
  }
}

assertThrows(() => {
  new ClassNonExtensibleWithIndexField(globalThis);
}, TypeError, /Cannot define property 0, object is not extensible/);
assertEquals("undefined", typeof nonExtensible);
