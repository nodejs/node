// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Create {count} property assignments.
function createPropertiesAssignment(count) {
  let result = "";
  for (let i = 0; i < count; i++) {
    result += "this.p"+i+" = undefined;";
  }
  return result;
}

function testSubclassProtoProperties(count) {
  const MyClass = eval(`(class MyClass {
    constructor() {
      ${createPropertiesAssignment(count)}
    }
  });`);

  class BaseClass {};
  class SubClass extends BaseClass {
    constructor() {
        super()
    }
  };

  const boundMyClass = MyClass.bind();
  %HeapObjectVerify(boundMyClass);

  SubClass.__proto__ = boundMyClass;
  var instance = new SubClass();

  %HeapObjectVerify(instance);
  // Create some more instances to complete in-object slack tracking.
  let results = [];
  for (let i = 0; i < 4000; i++) {
    results.push(new SubClass());
  }
  var instance = new SubClass();
  %HeapObjectVerify(instance);
}


for (let count = 0; count < 10; count++) {
  testSubclassProtoProperties(count);
}
