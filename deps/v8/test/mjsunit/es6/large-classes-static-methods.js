// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function testLargeClassesStaticMethods() {
  // This is to test for dictionary mode when there more than
  // kMaxNumberOfDescriptors (1024) properties.
  const kLimit = 1030;
  let evalString = "(function(i) { " +
      "let clazz = class { " +
      "   constructor(i) { this.value = i; } ";
  for (let i = 0; i < kLimit; i++) {
    evalString  += "static property"+i+"() { return "+i+" }; "
  }
  evalString += "};" +
      " return new clazz(i); })";

  let fn = eval(evalString);

  %PrepareFunctionForOptimization(fn);
  assertEquals(fn(1).value, 1);
  assertEquals(fn(2).value, 2);
  assertEquals(fn(3).value, 3);
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(fn(4).value, 4);

  let instance = fn(1);
  assertEquals(Object.getOwnPropertyNames(instance).length, 1);
  assertEquals(instance.value, 1);
  instance.value = 10;
  assertEquals(instance.value, 10);

  // kLimit + nof default properties (length, prototype, name).
  assertEquals(Object.getOwnPropertyNames(instance.constructor).length,
      kLimit + 3);

  // Call all static properties.
  for (let i = 0; i < kLimit; i++) {
    const key = "property" + i;
    assertEquals(instance.constructor[key](), i);
  }
})();
