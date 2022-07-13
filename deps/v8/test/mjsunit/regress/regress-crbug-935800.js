// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() {
  "use asm";
  function bar() {}
  return {bar: bar};
}
var module = foo();
assertTrue(Object.getOwnPropertyNames(module.bar).includes("prototype"));
assertInstanceof(new module.bar(), module.bar);
