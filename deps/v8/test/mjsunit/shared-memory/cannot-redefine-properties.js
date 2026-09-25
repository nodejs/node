// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

// Shared objects have fixed layout and cannot have their properties
// redefined. They are constructed sealed. In ordinary objects, sealed objects
// can be redefined to be frozen. This is disallowed for shared objects.

function TestSharedObjectCannotRedefineProperty(sharedObj) {
  function TestRedefine(sharedObj, propName) {
    if (Object.hasOwn(sharedObj, propName)) {
      assertThrows(() => Object.defineProperty(sharedObj, propName, {
        value: 99,
        enumerable: true,
        writable: false,
        configurable: false
      }));
      Object.defineProperty(
          sharedObj, propName,
          {value: 99, enumerable: true, writable: true, configurable: false});
      assertEquals(99, sharedObj[propName]);
    }
  }

  TestRedefine(sharedObj, 'p');
  TestRedefine(sharedObj, '0');
  // Objects without any properties are a degenerate case and are considered
  // frozen (or any integrity level, really).
  if (Object.getOwnPropertyNames(sharedObj).length > 0) {
    assertThrows(() => Object.freeze(sharedObj));
  }
  assertTrue(Object.isSealed(sharedObj));
}

TestSharedObjectCannotRedefineProperty(new Atomics.Condition());
TestSharedObjectCannotRedefineProperty(new Atomics.Mutex());
TestSharedObjectCannotRedefineProperty(new (new SharedStructType(['p'])));
TestSharedObjectCannotRedefineProperty(new SharedArray(1));
