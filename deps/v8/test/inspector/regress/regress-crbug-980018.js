// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Regression test for crbug.com/980018');

contextGroup.addInlineScript(`
class A extends Map {
}

class B extends Map {
  constructor() {
    super();
    Object.defineProperty(this, Symbol.toStringTag, {value: 'ThisIsB'});
  }
}

class C extends Map {
}
Object.defineProperty(C.prototype, Symbol.toStringTag, {value: 'ThisIsC'});`);

InspectorTest.runAsyncTestSuite([
  async function testClassNames() {
    await Protocol.Runtime.enable();
    await Promise.all(['A', 'B', 'C'].map(klass => Protocol.Runtime.evaluate({expression: `new ${klass}();`}).then(({result}) => InspectorTest.logMessage(result))));
    await Protocol.Runtime.disable();
  }
]);
