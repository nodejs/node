// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function testLargeClassesProperties(){
  // This is to test for dictionary mode when there more than
  // kMaxNumberOfDescriptors (1024) properties.
  const kLimit = 1030;
  let evalString = "(function(i) { " +
      "let clazz = class { " +
      "   constructor(i) { this.value = i;";
  for (let i = 0; i < kLimit ; i++) {
    evalString  += "this.property"+i +" = "+i+"; "
  }
  evalString += "}};" +
      " return (new clazz(i)); })";

  let fn = eval(evalString);
  %PrepareFunctionForOptimization(fn);
  assertEquals(fn(1).value, 1);
  assertEquals(fn(2).value, 2);
  assertEquals(fn(3).value, 3);
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(fn(4).value, 4);

  let instance = fn(1);
  assertEquals(Object.getOwnPropertyNames(instance).length, kLimit+1);

  // Get and set all properties.
  for (let i = 0; i < kLimit; i++) {
    const key = "property" + i;
    assertEquals(instance[key], i);
    const value = "value"+i;
    instance[key] = value;
    assertEquals(instance[key], value);
  }
})();
