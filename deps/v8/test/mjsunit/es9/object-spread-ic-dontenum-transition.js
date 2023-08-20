// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testMegamorphicWithDontEnumTransition() {
  function spread(o) { return { ...o }; }

  // Set up transition tree
  let obj = { ...{}, a: 0, b: 1, c: 2, };
  Object.defineProperty(obj, "boom", { enumerable: false, configurable: true,
                                       writable: true });

  // make CloneObjectIC MEGAMORPHIC
  spread(new Proxy({}, {}));

  // Ensure we don't crash, and create the correct object
  let result = spread({ a: 0, b: 1, c: 2, boom: 3 });

  assertEquals({ a: 0, b: 1, c: 2, boom: 3 }, result);
  assertEquals({
    enumerable: true,
    writable: true,
    configurable: true,
    value: 3,
  }, Object.getOwnPropertyDescriptor(result, "boom"));
})();
